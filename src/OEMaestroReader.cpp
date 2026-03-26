#include "oemaestro/OEMaestroReader.h"
#include "oemaestro/MaestroReader.h"
#include "oemaestro/MolConverter.h"
#include "oemaestro/StreamAdapter.h"
#include "oemaestro/Error.h"
#include "oemaestro/BoundedQueue.h"
#include <thread>
#include <map>
#include <variant>
#include <stdexcept>
#include <atomic>

namespace OEMaestro {

struct OEMaestroReader::Impl {
    std::unique_ptr<MaestroReader> reader;
    MolConverter converter;
    std::unique_ptr<OEChem::OEConfTestBase> conf_test;
    OEChem::OEMol pending_mol;
    bool has_pending = false;
    MaestroMol maestro_buf;

    unsigned int num_threads_ = 1;

    // Threading infrastructure (only used when num_threads_ > 1)
    using InputItem = std::pair<uint64_t, MaestroMol>;
    using OutputItem = std::pair<uint64_t, std::variant<OEChem::OEMol, std::exception_ptr>>;

    std::unique_ptr<BoundedQueue<InputItem>> input_queue_;
    std::unique_ptr<BoundedQueue<OutputItem>> output_queue_;
    std::thread producer_;
    std::vector<std::thread> workers_;
    std::map<uint64_t, std::variant<OEChem::OEMol, std::exception_ptr>> reorder_buf_;
    uint64_t next_seq_ = 0;
    std::shared_ptr<std::atomic<unsigned int>> active_workers_;

    Impl(const std::string& filename, OEMaestroReaderConfig config)
        : converter(config.GetTags(), config.GetPerception()),
          conf_test(std::make_unique<OEChem::OEDefaultConfTest>()),
          num_threads_((config.GetNumThreads() == 0) ? 1 : config.GetNumThreads()) {
        reader = std::make_unique<MaestroReader>(filename);
        if (num_threads_ > 1) StartThreads();
    }

    Impl(OEPlatform::oeifstream& ifs, OEMaestroReaderConfig config)
        : converter(config.GetTags(), config.GetPerception()),
          conf_test(std::make_unique<OEChem::OEDefaultConfTest>()),
          num_threads_((config.GetNumThreads() == 0) ? 1 : config.GetNumThreads()) {
        reader = std::make_unique<MaestroReader>(make_maeparser_stream(ifs));
        if (num_threads_ > 1) StartThreads();
    }

    ~Impl() {
        if (num_threads_ > 1) StopThreads();
    }

    void StartThreads() {
        size_t cap = 2 * num_threads_;
        input_queue_ = std::make_unique<BoundedQueue<InputItem>>(cap);
        output_queue_ = std::make_unique<BoundedQueue<OutputItem>>(cap);
        unsigned int num_workers = num_threads_ - 1;
        active_workers_ = std::make_shared<std::atomic<unsigned int>>(num_workers);

        producer_ = std::thread([this] {
            uint64_t seq = 0;
            MaestroMol buf;
            try {
                while (reader->Read(buf)) {
                    if (!input_queue_->Push({seq++, std::move(buf)})) break;
                    buf = MaestroMol{};
                }
            } catch (...) {
                output_queue_->Push({seq, std::current_exception()});
            }
            input_queue_->Close();
        });

        OEMaestroTag tags = converter.GetTagFormat();
        OEMaestroPerception perception = converter.GetPerception();
        auto active = active_workers_;
        auto* out_q = output_queue_.get();
        for (unsigned int i = 0; i < num_workers; i++) {
            workers_.emplace_back([this, tags, perception, active, out_q] {
                MolConverter local_converter(tags, perception);
                while (auto item = input_queue_->Pop()) {
                    auto& [seq, maestro_mol] = *item;
                    try {
                        OEChem::OEMol mol;
                        local_converter.Convert(maestro_mol, mol);
                        out_q->Push({seq, std::move(mol)});
                    } catch (...) {
                        out_q->Push({seq, std::current_exception()});
                    }
                }
                if (active->fetch_sub(1) == 1) {
                    out_q->Close();
                }
            });
        }
    }

    void StopThreads() {
        input_queue_->Close();
        output_queue_->Close();
        if (producer_.joinable()) producer_.join();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }

    bool ReadThreaded(OEChem::OEMol& mol) {
        while (true) {
            auto it = reorder_buf_.find(next_seq_);
            if (it != reorder_buf_.end()) {
                auto& val = it->second;
                if (std::holds_alternative<std::exception_ptr>(val)) {
                    auto eptr = std::get<std::exception_ptr>(val);
                    reorder_buf_.erase(it);
                    next_seq_++;
                    std::rethrow_exception(eptr);
                }
                mol = std::move(std::get<OEChem::OEMol>(val));
                reorder_buf_.erase(it);
                next_seq_++;
                return true;
            }
            auto item = output_queue_->Pop();
            if (!item) return false;
            auto& [seq, val] = *item;
            if (seq == next_seq_) {
                if (std::holds_alternative<std::exception_ptr>(val)) {
                    next_seq_++;
                    std::rethrow_exception(std::get<std::exception_ptr>(val));
                }
                mol = std::move(std::get<OEChem::OEMol>(val));
                next_seq_++;
                return true;
            }
            reorder_buf_.emplace(seq, std::move(val));
        }
    }

    bool Read(OEChem::OEMol& mol) {
        if (num_threads_ > 1) {
            if (!conf_test->HasCompareMols()) {
                return ReadThreaded(mol);
            }
            // Conformer grouping (post-reorder on main thread)
            if (has_pending) {
                mol = std::move(pending_mol);
                has_pending = false;
            } else {
                if (!ReadThreaded(mol)) return false;
            }
            while (true) {
                OEChem::OEMol next;
                if (!ReadThreaded(next)) break;
                if (conf_test->CompareMols(mol, next)) {
                    conf_test->CombineMols(mol, next);
                } else {
                    pending_mol = std::move(next);
                    has_pending = true;
                    return true;
                }
            }
            return true;
        }

        // Sequential path (unchanged)
        if (!conf_test->HasCompareMols()) {
            if (!reader->Read(maestro_buf))
                return false;
            converter.Convert(maestro_buf, mol);
            return true;
        }

        if (has_pending) {
            mol = pending_mol;
            has_pending = false;
        } else {
            if (!reader->Read(maestro_buf))
                return false;
            converter.Convert(maestro_buf, mol);
        }

        while (reader->Read(maestro_buf)) {
            pending_mol = OEChem::OEMol();
            converter.Convert(maestro_buf, pending_mol);
            if (conf_test->CompareMols(mol, pending_mol)) {
                conf_test->CombineMols(mol, pending_mol);
            } else {
                has_pending = true;
                return true;
            }
        }
        return true;
    }
};

OEMaestroReader::OEMaestroReader(const std::string& filename,
                                 OEMaestroReaderConfig config)
    : pimpl_(std::make_unique<Impl>(filename, config)) {}

OEMaestroReader::OEMaestroReader(OEPlatform::oeifstream& ifs,
                                 OEMaestroReaderConfig config)
    : pimpl_(std::make_unique<Impl>(ifs, config)) {}

bool OEMaestroReader::Read(OEChem::OEMol& mol) {
    return pimpl_->Read(mol);
}

bool OEMaestroReader::Read(OEChem::OEMolBase& mol) {
    if (pimpl_->num_threads_ > 1) {
        OEChem::OEMol temp;
        if (!pimpl_->ReadThreaded(temp)) return false;
        mol = temp;
        return true;
    }
    // Sequential path
    if (pimpl_->has_pending) {
        mol = pimpl_->pending_mol;
        pimpl_->has_pending = false;
        return true;
    }
    if (!pimpl_->reader->Read(pimpl_->maestro_buf))
        return false;
    pimpl_->converter.Convert(pimpl_->maestro_buf, mol);
    return true;
}

void OEMaestroReader::SetConfTest(OEChem::OEConfTestBase* conf_test) {
    if (conf_test) {
        pimpl_->conf_test.reset(conf_test);
    } else {
        pimpl_->conf_test = std::make_unique<OEChem::OEDefaultConfTest>();
    }
}

OEMaestroPerception OEMaestroReader::GetPerception() const {
    return pimpl_->converter.GetPerception();
}

OEMaestroTag OEMaestroReader::GetTagFormat() const {
    return pimpl_->converter.GetTagFormat();
}

OEMaestroReaderConfig OEMaestroReader::GetConfig() const {
    return OEMaestroReaderConfig(pimpl_->converter.GetTagFormat(), pimpl_->converter.GetPerception(), pimpl_->num_threads_);
}

OEMaestroReader::~OEMaestroReader() = default;
OEMaestroReader::OEMaestroReader(OEMaestroReader&&) noexcept = default;
OEMaestroReader& OEMaestroReader::operator=(OEMaestroReader&&) noexcept = default;

}  // namespace OEMaestro
