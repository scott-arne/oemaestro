#ifndef OEMAESTRO_OEREADMAESTRO_H
#define OEMAESTRO_OEREADMAESTRO_H

#include <string>

#include <oechem.h>

#include "oemaestro/Enums.h"
#include "oemaestro/OEMaestroReader.h"

namespace OEMaestro {

/// Creates a reader from a filename (C++ only, returns move-only iterator).
///
/// :param filename: Path to a Maestro file (.mae or .mae.gz).
/// :param config: Reader configuration for tag format and perception.
/// :returns: OEMaestroReader object for iteration.
OEMaestroReader OEReadMaestro(const std::string& filename,
                                OEMaestroReaderConfig config = {});

/// Creates a reader from an oeifstream (C++ only, returns move-only iterator).
///
/// :param ifs: An open OpenEye input file stream.
/// :param config: Reader configuration for tag format and perception.
/// :returns: OEMaestroReader object for iteration.
OEMaestroReader OEReadMaestro(OEPlatform::oeifstream& ifs,
                                OEMaestroReaderConfig config = {});

/// Reads a single molecule from a file.
///
/// :param filename: Path to a Maestro file (.mae or .mae.gz).
/// :param mol: OEMolBase to populate.
/// :param config: Reader configuration for tag format and perception.
/// :returns: True if a molecule was read, false at EOF or error.
bool OEReadMaestro(const std::string& filename, OEChem::OEMolBase& mol,
                    OEMaestroReaderConfig config = {});

/// Reads a single molecule from an oeifstream.
///
/// :param ifs: An open OpenEye input file stream.
/// :param mol: OEMolBase to populate.
/// :param config: Reader configuration for tag format and perception.
/// :returns: True if a molecule was read, false at EOF or error.
bool OEReadMaestro(OEPlatform::oeifstream& ifs, OEChem::OEMolBase& mol,
                    OEMaestroReaderConfig config = {});

}  // namespace OEMaestro

#endif  // OEMAESTRO_OEREADMAESTRO_H
