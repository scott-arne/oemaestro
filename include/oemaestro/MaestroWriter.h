#ifndef OEMAESTRO_MAESTROWRITER_H
#define OEMAESTRO_MAESTROWRITER_H

#include <memory>
#include <string>
#include "oemaestro/Enums.h"
#include "oemaestro/MaestroMol.h"

namespace OEMaestro {

/// Layer 1 writer: converts MaestroMol IR to Maestro file format.
///
/// Uses a pimpl pattern to keep maeparser (private dependency) out of
/// the public header. Supports plain text and gzip-compressed output.
class MaestroWriter {
public:
    /// Construct from filename. Compression inferred from extension.
    ///
    /// :param filename: Output file path (.mae or .mae.gz/.maegz).
    /// :param mode: WRITE_CREATE (default) or WRITE_APPEND.
    /// :raises OEMaestroError: If APPEND mode with compressed filename.
    /// :raises MaestroParseError: If file cannot be opened.
    explicit MaestroWriter(const std::string& filename,
                           OEMaestroWriteMode mode = WRITE_CREATE);

    /// Construct from shared ostream.
    ///
    /// :param stream: Output stream (caller retains ownership via shared_ptr).
    /// :param mode: WRITE_CREATE (default) or WRITE_APPEND.
    explicit MaestroWriter(std::shared_ptr<std::ostream> stream,
                           OEMaestroWriteMode mode = WRITE_CREATE);

    ~MaestroWriter();

    MaestroWriter(MaestroWriter&&) noexcept;
    MaestroWriter& operator=(MaestroWriter&&) noexcept;
    MaestroWriter(const MaestroWriter&) = delete;
    MaestroWriter& operator=(const MaestroWriter&) = delete;

    /// Write a single MaestroMol as one CT block.
    ///
    /// :param mol: The molecule IR to write.
    /// :returns: True on success, false on stream failure.
    /// :raises OEMaestroError: If writer is closed.
    bool Write(const MaestroMol& mol);

    /// Flush and close the underlying stream.
    void Close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_MAESTROWRITER_H
