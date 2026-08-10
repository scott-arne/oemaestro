#ifndef OEMAESTRO_READSTATUS_H
#define OEMAESTRO_READSTATUS_H

#include <string>

namespace OEMaestro {

/// Outcome of a single non-throwing record read.
enum class ReadStatus {
    Ok,           ///< A molecule was read.
    EndOfStream,  ///< No more CT blocks; the file ended cleanly.
    RecordError,  ///< A CT block failed to parse.
};

/// Result of OEMaestroReader::TryRead.
///
/// This is oemaestro's own type rather than oeio::ReadResult because the core
/// library builds with -DOEMAESTRO_BUILD_OEIO=OFF; only src/oeio_handler.cpp
/// translates between the two.
struct ReadResult {
    ReadStatus status = ReadStatus::EndOfStream;

    /// Parse diagnostic, populated only for RecordError.
    std::string message;

    /// Always false for Maestro. maeparser leaves no recoverable stream position
    /// after a failed CT block, so a record error ends the stream. The field
    /// exists so callers can write format-agnostic loops.
    bool resynchronized = false;

    /// \returns True only for Ok, so `if (result)` reads as "got a molecule".
    explicit operator bool() const { return status == ReadStatus::Ok; }
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_READSTATUS_H
