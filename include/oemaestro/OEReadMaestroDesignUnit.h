#ifndef OEMAESTRO_OEREADMAESTRODESIGNUNIT_H
#define OEMAESTRO_OEREADMAESTRODESIGNUNIT_H

#include <string>

#include <oechem.h>
#include <oebio.h>

#include "oemaestro/Enums.h"
#include "oemaestro/OEMaestroDesignUnitReader.h"

namespace OEMaestro {

/// Creates a design unit reader from a filename (C++ only, returns move-only object).
///
/// :param filename: Path to a Maestro file (.mae, .mae.gz, .maegz).
/// :param config: Reader configuration for tag format and perception.
/// :returns: OEMaestroDesignUnitReader object for iteration.
OEMaestroDesignUnitReader OEReadMaestroDesignUnit(
    const std::string& filename, OEMaestroReaderConfig config = {});

/// Creates a design unit reader from an oeifstream (C++ only, returns move-only object).
///
/// :param ifs: An open OpenEye input file stream.
/// :param config: Reader configuration for tag format and perception.
/// :returns: OEMaestroDesignUnitReader object for iteration.
OEMaestroDesignUnitReader OEReadMaestroDesignUnit(
    OEPlatform::oeifstream& ifs, OEMaestroReaderConfig config = {});

/// Reads a single design unit from a file.
///
/// :param filename: Path to a Maestro file (.mae, .mae.gz, .maegz).
/// :param du: OEDesignUnit to populate.
/// :param config: Reader configuration for tag format and perception.
/// :returns: True if a design unit was read, false at EOF or error.
bool OEReadMaestroDesignUnit(const std::string& filename,
                              OEBio::OEDesignUnit& du,
                              OEMaestroReaderConfig config = {});

/// Reads a single design unit from an oeifstream.
///
/// :param ifs: An open OpenEye input file stream.
/// :param du: OEDesignUnit to populate.
/// :param config: Reader configuration for tag format and perception.
/// :returns: True if a design unit was read, false at EOF or error.
bool OEReadMaestroDesignUnit(OEPlatform::oeifstream& ifs,
                              OEBio::OEDesignUnit& du,
                              OEMaestroReaderConfig config = {});

}  // namespace OEMaestro

#endif  // OEMAESTRO_OEREADMAESTRODESIGNUNIT_H
