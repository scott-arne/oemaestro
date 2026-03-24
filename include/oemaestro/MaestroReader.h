#ifndef OEMAESTRO_MAESTROREADER_H
#define OEMAESTRO_MAESTROREADER_H

#include <istream>
#include <memory>
#include <string>

#include "oemaestro/MaestroMol.h"

namespace OEMaestro {

/// Reads Maestro format files (.mae, .mae.gz) into MaestroMol objects.
///
/// Wraps the maeparser library to sequentially read CT blocks from a Maestro
/// file. Each call to Read() populates a MaestroMol with atom coordinates,
/// bond connectivity, and associated properties.
///
/// Example::
///
///     MaestroReader reader("input.mae");
///     MaestroMol mol;
///     while (reader.Read(mol)) {
///         // process mol
///     }
class MaestroReader {
public:
    /// Constructs a reader from a filename.
    ///
    /// :param filename: Path to a Maestro file (.mae or .mae.gz).
    /// :raises MaestroParseError: If the file cannot be opened.
    explicit MaestroReader(const std::string& filename);

    /// Constructs a reader from an input stream.
    ///
    /// :param stream: Shared pointer to an input stream.
    /// :raises MaestroParseError: If the stream is invalid.
    explicit MaestroReader(std::shared_ptr<std::istream> stream);

    /// Reads the next CT block into a MaestroMol.
    ///
    /// Clears and repopulates the provided MaestroMol with data from the
    /// next CT block in the file. Returns false at end of file.
    ///
    /// :param mol: MaestroMol to populate.
    /// :returns: True if a molecule was read, false at end of file.
    /// :raises MaestroParseError: If parsing fails.
    bool Read(MaestroMol& mol);

    ~MaestroReader();

    MaestroReader(const MaestroReader&) = delete;
    MaestroReader& operator=(const MaestroReader&) = delete;
    MaestroReader(MaestroReader&&) noexcept;
    MaestroReader& operator=(MaestroReader&&) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_MAESTROREADER_H
