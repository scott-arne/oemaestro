#ifndef OEMAESTRO_OEWRITEMAESTRO_H
#define OEMAESTRO_OEWRITEMAESTRO_H

#include <string>

#include <oechem.h>

#include "oemaestro/Enums.h"

namespace OEMaestro {

/// Writes a single molecule to a Maestro file (creates/overwrites).
///
/// :param filename: Output file path (.mae or .mae.gz/.maegz).
/// :param mol: The molecule to write.
/// :returns: True on success.
bool OEWriteMaestro(const std::string& filename, const OEChem::OEMolBase& mol);

/// Writes a single molecule to a Maestro file with configuration.
///
/// :param filename: Output file path (.mae or .mae.gz/.maegz).
/// :param mol: The molecule to write.
/// :param config: Writer configuration.
/// :returns: True on success.
bool OEWriteMaestro(const std::string& filename, const OEChem::OEMolBase& mol,
                    const OEMaestroWriterConfig& config);

/// Writes a single molecule to an OpenEye oeofstream.
///
/// :param ofs: An open OpenEye output file stream.
/// :param mol: The molecule to write.
/// :returns: True on success.
bool OEWriteMaestro(OEPlatform::oeofstream& ofs, const OEChem::OEMolBase& mol);

/// Writes a single molecule to an OpenEye oeofstream with configuration.
///
/// :param ofs: An open OpenEye output file stream.
/// :param mol: The molecule to write.
/// :param config: Writer configuration.
/// :returns: True on success.
bool OEWriteMaestro(OEPlatform::oeofstream& ofs, const OEChem::OEMolBase& mol,
                    const OEMaestroWriterConfig& config);

}  // namespace OEMaestro

#endif  // OEMAESTRO_OEWRITEMAESTRO_H
