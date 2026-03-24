#include "oemaestro/OEReadMaestro.h"
#include "oemaestro/MaestroReader.h"
#include "oemaestro/MolConverter.h"
#include "oemaestro/StreamAdapter.h"

namespace OEMaestro {

OEMaestroReader OEReadMaestro(const std::string& filename,
                                OEMaestroReaderConfig config) {
    return OEMaestroReader(filename, config);
}

OEMaestroReader OEReadMaestro(OEPlatform::oeifstream& ifs,
                                OEMaestroReaderConfig config) {
    return OEMaestroReader(ifs, config);
}

bool OEReadMaestro(const std::string& filename, OEChem::OEMolBase& mol,
                    OEMaestroReaderConfig config) {
    MaestroReader reader(filename);
    MolConverter converter(config.tags, config.perception);
    MaestroMol maestro_mol;
    if (!reader.Read(maestro_mol))
        return false;
    converter.Convert(maestro_mol, mol);
    return true;
}

bool OEReadMaestro(OEPlatform::oeifstream& ifs, OEChem::OEMolBase& mol,
                    OEMaestroReaderConfig config) {
    StreamAdapter adapter(ifs);
    auto stream_ptr = std::shared_ptr<std::istream>(&adapter, [](std::istream*) {});
    MaestroReader reader(stream_ptr);
    MolConverter converter(config.tags, config.perception);
    MaestroMol maestro_mol;
    if (!reader.Read(maestro_mol))
        return false;
    converter.Convert(maestro_mol, mol);
    return true;
}

}  // namespace OEMaestro
