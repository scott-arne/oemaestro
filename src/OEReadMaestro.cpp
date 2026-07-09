#include "oemaestro/OEReadMaestro.h"
#include "oemaestro/MaestroReader.h"
#include "oemaestro/MolConverter.h"
#include "oemaestro/StreamAdapter.h"

namespace OEMaestro {

namespace {
bool IsGzipFilename(const std::string& f) {
    return (f.size() >= 7 && f.substr(f.size() - 7) == ".mae.gz") ||
           (f.size() >= 6 && f.substr(f.size() - 6) == ".maegz");
}
}  // namespace

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
    std::shared_ptr<std::istream> stream;
    OEPlatform::oeifstream ifs;
    if (IsGzipFilename(filename)) {
        stream = make_gzip_istream(filename);
    } else {
        ifs.open(filename);
        stream = make_maeparser_stream(ifs);
    }
    MaestroReader reader(stream);
    MolConverter converter(config.GetTags(), config.GetPerception());
    MaestroMol maestro_mol;
    if (!reader.Read(maestro_mol))
        return false;
    converter.ConvertToOE(mol, maestro_mol);
    return true;
}

bool OEReadMaestro(OEPlatform::oeifstream& ifs, OEChem::OEMolBase& mol,
                    OEMaestroReaderConfig config) {
    MaestroReader reader(make_maeparser_stream(ifs));
    MolConverter converter(config.GetTags(), config.GetPerception());
    MaestroMol maestro_mol;
    if (!reader.Read(maestro_mol))
        return false;
    converter.ConvertToOE(mol, maestro_mol);
    return true;
}

}  // namespace OEMaestro
