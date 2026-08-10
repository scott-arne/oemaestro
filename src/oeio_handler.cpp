/// \file oeio_handler.cpp
/// \brief oeio format handler plugin for Schrodinger Maestro files.

#include <oeio/format_handler.h>
#include <oeio/format_registry.h>
#include <oeio/read_status.h>

#include "oemaestro/OEMaestroReader.h"
#include "oemaestro/OEMaestroWriter.h"
#include "oemaestro/Enums.h"
#include "oemaestro/ReadStatus.h"

#include <oechem.h>
#include <oesystem.h>

#include <optional>

namespace {

class MaestroMolSource : public oeio::MolSource {
public:
    MaestroMolSource(const std::string& path, const std::any& config) {
        OEMaestro::OEMaestroReaderConfig cfg;
        if (config.has_value()) {
            try {
                cfg = std::any_cast<OEMaestro::OEMaestroReaderConfig>(config);
            } catch (const std::bad_any_cast&) {
                OESystem::OEThrow.Warning(
                    "oeio: Maestro reader received unexpected config type; "
                    "using defaults");
            }
        }
        reader_.emplace(path, cfg);
    }

    bool next(OEChem::OEGraphMol& mol) override {
        mol.Clear();
        return reader_->Read(mol);
    }

    bool next(OEChem::OEMolBase& mol) override {
        mol.Clear();
        return reader_->Read(mol);
    }

    bool can_resynchronize() const override { return reader_->CanResynchronize(); }

    oeio::ReadResult try_next(OEChem::OEMolBase& mol) override {
        mol.Clear();
        const OEMaestro::ReadResult result = reader_->TryRead(mol);
        switch (result.status) {
            case OEMaestro::ReadStatus::Ok:
                return oeio::read_ok();
            case OEMaestro::ReadStatus::EndOfStream:
                return oeio::read_end();
            case OEMaestro::ReadStatus::RecordError:
                return oeio::read_error(result.message, result.resynchronized);
        }
        return oeio::read_end();
    }

private:
    std::optional<OEMaestro::OEMaestroReader> reader_;
};

class MaestroMolSink : public oeio::MolSink {
public:
    MaestroMolSink(const std::string& path, const std::any& config) {
        OEMaestro::OEMaestroWriterConfig cfg;
        if (config.has_value()) {
            try {
                cfg = std::any_cast<OEMaestro::OEMaestroWriterConfig>(config);
            } catch (const std::bad_any_cast&) {
                OESystem::OEThrow.Warning(
                    "oeio: Maestro writer received unexpected config type; "
                    "using defaults");
            }
        }
        writer_.emplace(path, cfg);
    }

    bool write(const OEChem::OEMolBase& mol) override {
        return writer_->Write(mol);
    }

    void close() override {
        writer_->Close();
    }

private:
    std::optional<OEMaestro::OEMaestroWriter> writer_;
};

class MaestroFormatHandler : public oeio::FormatHandler {
public:
    oeio::FormatInfo info() const override {
        return {
            "Maestro",
            {".mae.gz", ".maegz", ".mae"},
            "Schrodinger Maestro format",
            true,   // supports_read
            true,   // supports_write
            false,  // supports_threaded_read
            false   // supports_threaded_write
        };
    }

    std::unique_ptr<oeio::MolSource> make_reader(
        const std::string& path, const std::any& config) const override
    {
        return std::make_unique<MaestroMolSource>(path, config);
    }

    std::unique_ptr<oeio::MolSink> make_writer(
        const std::string& path, const std::any& config) const override
    {
        return std::make_unique<MaestroMolSink>(path, config);
    }
};

}  // namespace

OEIO_REGISTER_FORMAT(MaestroFormatHandler)

namespace OEMaestro {
/// Dummy function referenced by the SWIG module to prevent the linker
/// from stripping oeio_handler.o (and its static OEIO_REGISTER_FORMAT
/// initializer) from the final shared library.
void oemaestro_force_link_oeio_handler() {}
}  // namespace OEMaestro
