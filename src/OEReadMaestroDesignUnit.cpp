#include "oemaestro/OEReadMaestroDesignUnit.h"

namespace OEMaestro {

OEMaestroDesignUnitReader OEReadMaestroDesignUnit(
    const std::string& filename, OEMaestroReaderConfig config) {
    return OEMaestroDesignUnitReader(filename, config);
}

OEMaestroDesignUnitReader OEReadMaestroDesignUnit(
    OEPlatform::oeifstream& ifs, OEMaestroReaderConfig config) {
    return OEMaestroDesignUnitReader(ifs, config);
}

bool OEReadMaestroDesignUnit(const std::string& filename,
                              OEBio::OEDesignUnit& du,
                              OEMaestroReaderConfig config) {
    OEMaestroDesignUnitReader reader(filename, config);
    return reader.Read(du);
}

bool OEReadMaestroDesignUnit(OEPlatform::oeifstream& ifs,
                              OEBio::OEDesignUnit& du,
                              OEMaestroReaderConfig config) {
    OEMaestroDesignUnitReader reader(ifs, config);
    return reader.Read(du);
}

}  // namespace OEMaestro
