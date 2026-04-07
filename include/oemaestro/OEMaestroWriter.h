#ifndef OEMAESTRO_OEMAESTROWRITER_H
#define OEMAESTRO_OEMAESTROWRITER_H

#include <string>

#include <oechem.h>

#include "oemaestro/Enums.h"
#include "oemaestro/MaestroWriter.h"
#include "oemaestro/MolConverter.h"

namespace OEMaestro {

/// High-level writer that converts OpenEye molecules to Maestro format.
///
/// Multi-conformer OEMol molecules emit one CT block per conformer.
/// Uses PERCEPTION_NONE internally — writes only what is already on the molecule.
///
/// Example::
///
///     OEMaestroWriter writer("output.mae");
///     OEChem::OEGraphMol mol;
///     // ... populate mol ...
///     writer.Write(mol);
///     writer.Close();
class OEMaestroWriter {
public:
    /// Constructs a writer from a filename.
    ///
    /// :param filename: Output file path (.mae or .mae.gz/.maegz).
    /// :param mode: WRITE_CREATE (default) or WRITE_APPEND.
    explicit OEMaestroWriter(const std::string& filename,
                             OEMaestroWriteMode mode = WRITE_CREATE);

    /// Constructs a writer from a filename with configuration.
    ///
    /// :param filename: Output file path (.mae or .mae.gz/.maegz).
    /// :param config: Writer configuration.
    OEMaestroWriter(const std::string& filename,
                    const OEMaestroWriterConfig& config);

    /// Constructs a writer from an OpenEye oeofstream.
    ///
    /// :param ofs: An open OpenEye output file stream.
    /// :param mode: WRITE_CREATE (default) or WRITE_APPEND.
    explicit OEMaestroWriter(OEPlatform::oeofstream& ofs,
                             OEMaestroWriteMode mode = WRITE_CREATE);

    /// Constructs a writer from an OpenEye oeofstream with configuration.
    ///
    /// :param ofs: An open OpenEye output file stream.
    /// :param config: Writer configuration.
    OEMaestroWriter(OEPlatform::oeofstream& ofs,
                    const OEMaestroWriterConfig& config);

    ~OEMaestroWriter();

    OEMaestroWriter(OEMaestroWriter&&) noexcept;
    OEMaestroWriter& operator=(OEMaestroWriter&&) noexcept;
    OEMaestroWriter(const OEMaestroWriter&) = delete;
    OEMaestroWriter& operator=(const OEMaestroWriter&) = delete;

    /// Writes a molecule. Multi-conformer molecules emit one CT per conformer.
    ///
    /// :param mol: The molecule to write.
    /// :returns: True on success.
    bool Write(const OEChem::OEMolBase& mol);

    /// Flushes and closes the underlying stream.
    void Close();

private:
    MolConverter converter_;
    MaestroWriter writer_;
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_OEMAESTROWRITER_H
