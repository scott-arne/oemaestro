#ifndef OEMAESTRO_ERROR_H
#define OEMAESTRO_ERROR_H

#include <stdexcept>
#include <string>

namespace OEMaestro {

/// Base exception for oemaestro errors.
class OEMaestroError : public std::runtime_error {
public:
    explicit OEMaestroError(const std::string& message)
        : std::runtime_error(message) {}
};

/// Thrown when a Maestro file cannot be opened or is malformed.
class MaestroParseError : public OEMaestroError {
public:
    explicit MaestroParseError(const std::string& message)
        : OEMaestroError(message) {}
};

/// Thrown when molecule conversion fails (e.g., invalid bond indices).
class MaestroConvertError : public OEMaestroError {
public:
    explicit MaestroConvertError(const std::string& message)
        : OEMaestroError(message) {}
};

}  // namespace OEMaestro

#endif  // OEMAESTRO_ERROR_H
