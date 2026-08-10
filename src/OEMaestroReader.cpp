#include "oemaestro/OEMaestroReader.h"
#include "oemaestro/MaestroReader.h"
#include "oemaestro/MolConverter.h"
#include "oemaestro/StreamAdapter.h"
#include "oemaestro/Error.h"
#include "oemaestro/BoundedQueue.h"
#include "oemaestro/ReadStatus.h"
#include <thread>
#include <map>
#include <variant>
#include <stdexcept>
#include <atomic>

namespace OEMaestro {

struct OEMaestroReader::Impl {
    std::unique_ptr<OEPlatform::oeifstream> owned_ifs_;
    std::unique_ptr<MaestroReader> reader;
    MolConverter converter;
    std::unique_ptr<OEChem::OEConfTestBase> conf_test;
    OEChem::OEMol pending_mol;
    bool has_pending = false;
    MaestroMol maestro_buf;

    // Terminal-state latch for TryRead. Set when a RecordError occurs; causes
    // subsequent TryRead calls to return EndOfStream immediately without
    // consulting the underlying reader. The throwing/boolean Read path does
    // not consult this latch (backward compatibility isolation).
    bool failed_ = false;

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
        if (is_gzip_filename(filename)) {
            reader = std::make_unique<MaestroReader>(make_gzip_istream(filename));
        } else {
            owned_ifs_ = std::make_unique<OEPlatform::oeifstream>(filename);
            reader = std::make_unique<MaestroReader>(make_maeparser_stream(*owned_ifs_));
        }
        if (num_threads_ > 1) StartThreads();  // NOLINT(readability-simplify-boolean-expr)
    }

    Impl(OEPlatform::oeifstream& ifs, OEMaestroReaderConfig config)
        : converter(config.GetTags(), config.GetPerception()),
          conf_test(std::make_unique<OEChem::OEDefaultConfTest>()),
          num_threads_((config.GetNumThreads() == 0) ? 1 : config.GetNumThreads()) {
        reader = std::make_unique<MaestroReader>(make_maeparser_stream(ifs));
        if (num_threads_ > 1) StartThreads();  // NOLINT(readability-simplify-boolean-expr)
    }

    ~Impl() {
        if (num_threads_ > 1) StopThreads();  // NOLINT(readability-simplify-boolean-expr)
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

        OEMaestroTag tags = converter.GetTagFormat();          // NOLINT -- captured by lambda below
        OEMaestroPerception perception = converter.GetPerception();  // NOLINT -- captured by lambda below
        auto active = active_workers_;
        auto* out_q = output_queue_.get();  // NOLINT -- captured by lambda below
        for (unsigned int i = 0; i < num_workers; i++) {
            workers_.emplace_back([this, tags, perception, active, out_q] {
                const MolConverter local_converter(tags, perception);
                while (auto item = input_queue_->Pop()) {
                    auto& [seq, maestro_mol] = *item;
                    try {
                        OEChem::OEMol mol;
                        local_converter.ConvertToOE(mol, maestro_mol);
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
                mol = std::get<OEChem::OEMol>(val);
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
                mol = std::get<OEChem::OEMol>(val);
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
                mol = pending_mol;  // NOLINT(performance-move-const-arg) OEMol has no move operator
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
                    pending_mol = next;  // OEMol has no move operator
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
            converter.ConvertToOE(mol, maestro_buf);
            return true;
        }

        if (has_pending) {
            mol = pending_mol;
            has_pending = false;
        } else {
            if (!reader->Read(maestro_buf))
                return false;
            converter.ConvertToOE(mol, maestro_buf);
        }

        while (reader->Read(maestro_buf)) {
            pending_mol = OEChem::OEMol();
            converter.ConvertToOE(pending_mol, maestro_buf);
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
    pimpl_->converter.ConvertToOE(mol, pimpl_->maestro_buf);
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
    return OEMaestroReaderConfig(pimpl_->converter.GetTagFormat(),  // NOLINT(modernize-return-braced-init-list)
                                pimpl_->converter.GetPerception(), pimpl_->num_threads_);
}


ReadResult OEMaestroReader::TryRead(OEChem::OEMol& mol) {
    // Stage through a local temporary so that failure never contaminates the
    // caller's mol. Cost: one molecule copy per successful read, paid for
    // correctness (imports are per-record and I/O-bound, so correctness wins).
    //
    // The try/catch covers ONLY Read(temp), not the final mol=temp commit.
    // If the destination copy throws (allocation failure, a throwing copy path),
    // that exception propagates — consistent with the documented contract
    // ("TryRead converts parse and I/O errors, not foreign throws"). The record
    // was already consumed from the stream, so it is lost, but the stream itself
    // remains readable (failed_ is NOT set, so no false terminal state).
    // The caller's molecule state after a copy failure is defined by OEChem's
    // assignment semantics, not by TryRead.
    if (pimpl_->failed_) {
        return ReadResult{ReadStatus::EndOfStream, {}, false};
    }
    OEChem::OEMol temp;
    bool ok = false;
    try {
        ok = Read(temp);
    } catch (const OEMaestroError& e) {
        pimpl_->failed_ = true;
        return ReadResult{ReadStatus::RecordError, e.what(), false};
    } catch (const std::exception& e) {
        pimpl_->failed_ = true;
        return ReadResult{ReadStatus::RecordError, e.what(), false};
    }
    // Commit the staged read AFTER the catch block so a copy failure cannot
    // masquerade as a record problem (it propagates; failed_ stays false).
    if (ok) {
        mol = temp;
        return ReadResult{ReadStatus::Ok, {}, false};
    }
    return ReadResult{ReadStatus::EndOfStream, {}, false};
}

ReadResult OEMaestroReader::TryRead(OEChem::OEMolBase& mol) {
    // Stage through a concrete local (OEGraphMol is an OEMolBase) to preserve
    // the overload's semantics (no conformer grouping). On success, assign via
    // base-level assignment. Cost: one molecule copy per successful read.
    //
    // The try/catch covers ONLY Read(temp), not the final mol=temp commit.
    // If the destination copy throws, that exception propagates (the record
    // was consumed but lost; the stream remains readable, failed_ stays false).
    // The caller's molecule state after a copy failure is defined by OEChem's
    // OEMolBase::operator= semantics, not by TryRead.
    if (pimpl_->failed_) {
        return ReadResult{ReadStatus::EndOfStream, {}, false};
    }
    OEChem::OEGraphMol temp;
    bool ok = false;
    try {
        ok = Read(static_cast<OEChem::OEMolBase&>(temp));
    } catch (const OEMaestroError& e) {
        pimpl_->failed_ = true;
        return ReadResult{ReadStatus::RecordError, e.what(), false};
    } catch (const std::exception& e) {
        pimpl_->failed_ = true;
        return ReadResult{ReadStatus::RecordError, e.what(), false};
    }
    // Commit the staged read AFTER the catch block so a copy failure cannot
    // masquerade as a record problem (it propagates; failed_ stays false).
    if (ok) {
        mol = temp;
        return ReadResult{ReadStatus::Ok, {}, false};
    }
    return ReadResult{ReadStatus::EndOfStream, {}, false};
}

bool OEMaestroReader::CanResynchronize() const { return false; }

OEMaestroReader::~OEMaestroReader() = default;
OEMaestroReader::OEMaestroReader(OEMaestroReader&&) noexcept = default;
OEMaestroReader& OEMaestroReader::operator=(OEMaestroReader&&) noexcept = default;

}  // namespace OEMaestro