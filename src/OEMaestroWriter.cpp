#include "oemaestro/OEMaestroWriter.h"
#include "oemaestro/StreamAdapter.h"
#include "oemaestro/Error.h"

namespace OEMaestro {

namespace {

bool IsGzipFilename(const std::string& filename) {
    if (filename.size() >= 7 && filename.substr(filename.size() - 7) == ".mae.gz")
        return true;
    if (filename.size() >= 6 && filename.substr(filename.size() - 6) == ".maegz")
        return true;
    return false;
}

}  // namespace

OEMaestroWriter::OEMaestroWriter(const std::string& filename, OEMaestroWriteMode mode)
    : converter_(TAG_ALL, PERCEPTION_NONE) {
    if (IsGzipFilename(filename)) {
        if (mode == WRITE_APPEND) {
            throw OEMaestroError("APPEND mode is not supported for compressed files");
        }
        writer_ = std::make_unique<MaestroWriter>(make_gzip_ostream(filename), mode);
    } else {
        writer_ = std::make_unique<MaestroWriter>(filename, mode);
    }
}

OEMaestroWriter::OEMaestroWriter(const std::string& filename,
                                 const OEMaestroWriterConfig& config)
    : converter_(config.GetTags(), PERCEPTION_NONE) {
    OEMaestroWriteMode mode = config.GetMode();
    if (IsGzipFilename(filename)) {
        if (mode == WRITE_APPEND) {
            throw OEMaestroError("APPEND mode is not supported for compressed files");
        }
        writer_ = std::make_unique<MaestroWriter>(make_gzip_ostream(filename), mode);
    } else {
        writer_ = std::make_unique<MaestroWriter>(filename, mode);
    }
}

OEMaestroWriter::OEMaestroWriter(OEPlatform::oeofstream& ofs, OEMaestroWriteMode mode)
    : converter_(TAG_ALL, PERCEPTION_NONE),
      writer_(std::make_unique<MaestroWriter>(make_maeparser_ostream(ofs), mode)) {}

OEMaestroWriter::OEMaestroWriter(OEPlatform::oeofstream& ofs,
                                 const OEMaestroWriterConfig& config)
    : converter_(config.GetTags(), PERCEPTION_NONE),
      writer_(std::make_unique<MaestroWriter>(make_maeparser_ostream(ofs), config.GetMode())) {}

OEMaestroWriter::~OEMaestroWriter() = default;
OEMaestroWriter::OEMaestroWriter(OEMaestroWriter&&) noexcept = default;
OEMaestroWriter& OEMaestroWriter::operator=(OEMaestroWriter&&) noexcept = default;

bool OEMaestroWriter::Write(const OEChem::OEMolBase& mol) {
    std::vector<MaestroMol> mmols;
    converter_.ConvertToMaestro(mmols, mol);
    for (const auto& mmol : mmols) {  // NOLINT(readability-use-anyofallof)
        if (!writer_->Write(mmol)) return false;
    }
    return true;
}

void OEMaestroWriter::Close() {
    writer_->Close();
}

}  // namespace OEMaestro
