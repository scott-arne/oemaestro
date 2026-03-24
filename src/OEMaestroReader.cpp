#include "oemaestro/OEMaestroReader.h"
#include "oemaestro/MaestroReader.h"
#include "oemaestro/MolConverter.h"
#include "oemaestro/StreamAdapter.h"
#include "oemaestro/Error.h"

namespace OEMaestro {

struct OEMaestroReader::Impl {
    std::unique_ptr<MaestroReader> reader;
    MolConverter converter;
    OEChem::OEConfTestBase* conf_test;  // Owned
    OEChem::OEMol pending_mol;
    bool has_pending = false;
    MaestroMol maestro_buf;
    // Keep StreamAdapter alive for oeifstream path
    std::unique_ptr<StreamAdapter> stream_adapter;

    Impl(const std::string& filename, OEMaestroReaderConfig config)
        : converter(config.tags, config.perception),
          conf_test(new OEChem::OEDefaultConfTest()) {
        reader = std::make_unique<MaestroReader>(filename);
    }

    Impl(OEPlatform::oeifstream& ifs, OEMaestroReaderConfig config)
        : converter(config.tags, config.perception),
          conf_test(new OEChem::OEDefaultConfTest()) {
        stream_adapter = std::make_unique<StreamAdapter>(ifs);
        // Create a shared_ptr that doesn't delete (adapter is owned by us)
        auto stream_ptr = std::shared_ptr<std::istream>(
            stream_adapter.get(), [](std::istream*) {});
        reader = std::make_unique<MaestroReader>(stream_ptr);
    }

    ~Impl() { delete conf_test; }

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
    if (!pimpl_->reader->Read(pimpl_->maestro_buf))
        return false;
    pimpl_->converter.Convert(pimpl_->maestro_buf, mol);
    return true;
}

void OEMaestroReader::SetConfTest(OEChem::OEConfTestBase* conf_test) {
    delete pimpl_->conf_test;
    pimpl_->conf_test = conf_test ? conf_test : new OEChem::OEDefaultConfTest();
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
    return {pimpl_->converter.GetTagFormat(), pimpl_->converter.GetPerception()};
}

OEMaestroReader::~OEMaestroReader() = default;
OEMaestroReader::OEMaestroReader(OEMaestroReader&&) noexcept = default;
OEMaestroReader& OEMaestroReader::operator=(OEMaestroReader&&) noexcept = default;

}  // namespace OEMaestro
