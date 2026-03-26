#include "oemaestro/OEMaestroReader.h"
#include "oemaestro/MaestroReader.h"
#include "oemaestro/MolConverter.h"
#include "oemaestro/StreamAdapter.h"
#include "oemaestro/Error.h"

namespace OEMaestro {

struct OEMaestroReader::Impl {
    std::unique_ptr<MaestroReader> reader;
    MolConverter converter;
    std::unique_ptr<OEChem::OEConfTestBase> conf_test;
    OEChem::OEMol pending_mol;
    bool has_pending = false;
    MaestroMol maestro_buf;
    unsigned int num_threads_ = 1;

    Impl(const std::string& filename, OEMaestroReaderConfig config)
        : converter(config.tags, config.perception),
          conf_test(std::make_unique<OEChem::OEDefaultConfTest>()) {
        num_threads_ = (config.num_threads == 0) ? 1 : config.num_threads;
        reader = std::make_unique<MaestroReader>(filename);
    }

    Impl(OEPlatform::oeifstream& ifs, OEMaestroReaderConfig config)
        : converter(config.tags, config.perception),
          conf_test(std::make_unique<OEChem::OEDefaultConfTest>()) {
        num_threads_ = (config.num_threads == 0) ? 1 : config.num_threads;
        reader = std::make_unique<MaestroReader>(make_maeparser_stream(ifs));
    }

    bool Read(OEChem::OEMol& mol) {
        // Default conf test -- no grouping, simple pass-through
        if (!conf_test->HasCompareMols()) {
            if (!reader->Read(maestro_buf))
                return false;
            converter.Convert(maestro_buf, mol);
            return true;
        }

        // Non-default conf test -- conformer grouping with lookahead
        if (has_pending) {
            mol = pending_mol;
            has_pending = false;
        } else {
            if (!reader->Read(maestro_buf))
                return false;
            converter.Convert(maestro_buf, mol);
        }

        // Lookahead loop: keep reading CTs and grouping conformers
        while (reader->Read(maestro_buf)) {
            pending_mol = OEChem::OEMol();
            converter.Convert(maestro_buf, pending_mol);

            if (conf_test->CompareMols(mol, pending_mol)) {
                // Same molecule -- add as conformer
                conf_test->CombineMols(mol, pending_mol);
            } else {
                // Different molecule -- save for next call
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
    // Consume any pending molecule left by conformer grouping lookahead
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

void OEMaestroReader::SetPerception(OEMaestroPerception perception) {
    pimpl_->converter.SetPerception(perception);
}

void OEMaestroReader::SetTagFormat(OEMaestroTag tags) {
    pimpl_->converter.SetTagFormat(tags);
}

OEMaestroPerception OEMaestroReader::GetPerception() const {
    return pimpl_->converter.GetPerception();
}

OEMaestroTag OEMaestroReader::GetTagFormat() const {
    return pimpl_->converter.GetTagFormat();
}

OEMaestroReaderConfig OEMaestroReader::GetConfig() const {
    return {pimpl_->converter.GetTagFormat(), pimpl_->converter.GetPerception(), pimpl_->num_threads_};
}

OEMaestroReader::~OEMaestroReader() = default;
OEMaestroReader::OEMaestroReader(OEMaestroReader&&) noexcept = default;
OEMaestroReader& OEMaestroReader::operator=(OEMaestroReader&&) noexcept = default;

}  // namespace OEMaestro
