#include "oemaestro/OEWriteMaestro.h"
#include "oemaestro/OEMaestroWriter.h"

namespace OEMaestro {

bool OEWriteMaestro(const std::string& filename, const OEChem::OEMolBase& mol) {
    OEMaestroWriter writer(filename);
    bool ok = writer.Write(mol);
    writer.Close();
    return ok;
}

bool OEWriteMaestro(const std::string& filename, const OEChem::OEMolBase& mol,
                    const OEMaestroWriterConfig& config) {
    OEMaestroWriter writer(filename, config);
    bool ok = writer.Write(mol);
    writer.Close();
    return ok;
}

bool OEWriteMaestro(OEPlatform::oeofstream& ofs, const OEChem::OEMolBase& mol) {
    OEMaestroWriter writer(ofs);
    bool ok = writer.Write(mol);
    writer.Close();
    return ok;
}

bool OEWriteMaestro(OEPlatform::oeofstream& ofs, const OEChem::OEMolBase& mol,
                    const OEMaestroWriterConfig& config) {
    OEMaestroWriter writer(ofs, config);
    bool ok = writer.Write(mol);
    writer.Close();
    return ok;
}

}  // namespace OEMaestro
