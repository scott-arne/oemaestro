#include "oemaestro/OEMaestroWriter.h"
#include "oemaestro/StreamAdapter.h"

namespace OEMaestro {

OEMaestroWriter::OEMaestroWriter(const std::string& filename, OEMaestroWriteMode mode)
    : converter_(TAG_ALL, PERCEPTION_NONE), writer_(filename, mode) {}

OEMaestroWriter::OEMaestroWriter(const std::string& filename,
                                 const OEMaestroWriterConfig& config)
    : converter_(config.GetTags(), PERCEPTION_NONE),
      writer_(filename, config.GetMode()) {}

OEMaestroWriter::OEMaestroWriter(OEPlatform::oeofstream& ofs, OEMaestroWriteMode mode)
    : converter_(TAG_ALL, PERCEPTION_NONE),
      writer_(make_maeparser_ostream(ofs), mode) {}

OEMaestroWriter::OEMaestroWriter(OEPlatform::oeofstream& ofs,
                                 const OEMaestroWriterConfig& config)
    : converter_(config.GetTags(), PERCEPTION_NONE),
      writer_(make_maeparser_ostream(ofs), config.GetMode()) {}

OEMaestroWriter::~OEMaestroWriter() = default;
OEMaestroWriter::OEMaestroWriter(OEMaestroWriter&&) noexcept = default;
OEMaestroWriter& OEMaestroWriter::operator=(OEMaestroWriter&&) noexcept = default;

bool OEMaestroWriter::Write(const OEChem::OEMolBase& mol) {
    std::vector<MaestroMol> mmols;
    converter_.Convert(mmols, mol);
    for (const auto& mmol : mmols) {  // NOLINT(readability-use-anyofallof)
        if (!writer_.Write(mmol)) return false;
    }
    return true;
}

void OEMaestroWriter::Close() {
    writer_.Close();
}

}  // namespace OEMaestro
