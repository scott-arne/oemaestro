#ifndef OEMAESTRO_STREAMADAPTER_H
#define OEMAESTRO_STREAMADAPTER_H

#include <istream>
#include <memory>
#include <string>
#include <vector>

#include <zlib.h>

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

/// Adapts OEPlatform::oeofstream to std::ostream for use with maeparser Writer.
///
/// This streambuf adapter bridges OpenEye's oeofstream to std::ostream
/// so it can be passed to maeparser's Writer which requires shared_ptr<std::ostream>.
class OEOutputStreamBuf : public std::streambuf {
public:
    /// Constructs an output streambuf adapter.
    ///
    /// :param ofs: The OpenEye output file stream to adapt.
    /// :param buf_size: Size of the internal buffer (default: 8192 bytes).
    explicit OEOutputStreamBuf(OEPlatform::oeofstream& ofs,
                               std::size_t buf_size = 8192);

protected:
    int overflow(int ch) override;
    std::streamsize xsputn(const char* s, std::streamsize count) override;
    int sync() override;

private:
    OEPlatform::oeofstream& ofs_;
    std::vector<char> buffer_;
    bool FlushBuffer();
};

/// Convenience wrapper: owns the streambuf and provides std::ostream interface.
class OutputStreamAdapter : public std::ostream {
public:
    /// Constructs an ostream adapter.
    ///
    /// :param ofs: The OpenEye output file stream to adapt.
    explicit OutputStreamAdapter(OEPlatform::oeofstream& ofs);

private:
    OEOutputStreamBuf buf_;
};

/// Creates a shared_ptr<std::ostream> from an oeofstream for use with maeparser Writer.
///
/// :param ofs: The OpenEye output file stream to adapt.
/// :returns: Shared pointer to an ostream wrapping the oeofstream.
std::shared_ptr<std::ostream> make_maeparser_ostream(OEPlatform::oeofstream& ofs);

// --- Gzip stream adapters using zlib (universal2-safe) ---

/// Streambuf that reads from a gzip file via zlib.
class GzipInputBuf : public std::streambuf {
public:
    explicit GzipInputBuf(const std::string& filename, size_t buf_size = 4096);
    ~GzipInputBuf() override;

    GzipInputBuf(const GzipInputBuf&) = delete;
    GzipInputBuf& operator=(const GzipInputBuf&) = delete;

protected:
    int_type underflow() override;

private:
    gzFile gz_;
    std::vector<char> buffer_;
};

/// istream that reads from a gzip file.
class GzipInputStream : public std::istream {
public:
    explicit GzipInputStream(const std::string& filename);

private:
    GzipInputBuf buf_;
};

/// Creates a shared_ptr<istream> for a gzip file, suitable for maeparser.
std::shared_ptr<std::istream> make_gzip_istream(const std::string& filename);

/// Streambuf that writes to a gzip file via zlib.
class GzipOutputBuf : public std::streambuf {
public:
    explicit GzipOutputBuf(const std::string& filename, size_t buf_size = 8192);
    ~GzipOutputBuf() override;

    GzipOutputBuf(const GzipOutputBuf&) = delete;
    GzipOutputBuf& operator=(const GzipOutputBuf&) = delete;

protected:
    int overflow(int ch) override;
    std::streamsize xsputn(const char* s, std::streamsize count) override;
    int sync() override;

private:
    gzFile gz_;
    std::vector<char> buffer_;
    bool FlushBuffer();
};

/// ostream that writes to a gzip file.
class GzipOutputStream : public std::ostream {
public:
    explicit GzipOutputStream(const std::string& filename);
    ~GzipOutputStream() override;

private:
    GzipOutputBuf buf_;
};

/// Creates a shared_ptr<ostream> for a gzip file, suitable for maeparser.
std::shared_ptr<std::ostream> make_gzip_ostream(const std::string& filename);

}  // namespace OEMaestro

#endif  // OEMAESTRO_STREAMADAPTER_H
