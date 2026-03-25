#ifndef OEMAESTRO_STREAMADAPTER_H
#define OEMAESTRO_STREAMADAPTER_H

#include <istream>
#include <memory>
#include <vector>

#include <oechem.h>

namespace OEMaestro {

/// Adapts OEPlatform::oeifstream to std::istream for use with maeparser.
///
/// This streambuf adapter bridges OpenEye's oeifstream to std::istream
/// so it can be passed to maeparser's Reader which requires shared_ptr<std::istream>.
class OEStreamBuf : public std::streambuf {
public:
    /// Constructs a streambuf adapter.
    ///
    /// :param ifs: The OpenEye input file stream to adapt.
    /// :param buf_size: Size of the internal buffer (default: 4096 bytes).
    explicit OEStreamBuf(OEPlatform::oeifstream& ifs, size_t buf_size = 4096);

protected:
    /// Refills the buffer when it's exhausted.
    ///
    /// :returns: The next character, or EOF if no more data available.
    int_type underflow() override;

private:
    OEPlatform::oeifstream& ifs_;
    std::vector<char> buffer_;
};

/// Convenience wrapper: owns the streambuf and provides std::istream interface.
///
/// This class makes it easy to use an oeifstream as a std::istream by managing
/// the streambuf internally.
class StreamAdapter : public std::istream {
public:
    /// Constructs an istream adapter.
    ///
    /// :param ifs: The OpenEye input file stream to adapt.
    explicit StreamAdapter(OEPlatform::oeifstream& ifs);

private:
    OEStreamBuf buf_;
};

/// Creates a shared_ptr<std::istream> from an oeifstream for use with maeparser.
///
/// The returned shared_ptr owns the underlying StreamAdapter, keeping it alive
/// as long as the maeparser Reader holds the pointer.
///
/// :param ifs: The OpenEye input file stream to adapt.
/// :returns: Shared pointer to an istream wrapping the oeifstream.
std::shared_ptr<std::istream> make_maeparser_stream(OEPlatform::oeifstream& ifs);

}  // namespace OEMaestro

#endif  // OEMAESTRO_STREAMADAPTER_H
