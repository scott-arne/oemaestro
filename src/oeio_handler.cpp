/// \file oeio_handler.cpp
/// \brief oeio format handler plugin for Schrodinger Maestro files.

#include <oeio/format_handler.h>
#include <oeio/format_registry.h>

#include "oemaestro/OEMaestroReader.h"
#include "oemaestro/OEMaestroWriter.h"
#include "oemaestro/Enums.h"

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
