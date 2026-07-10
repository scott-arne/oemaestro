#include "oemaestro/StreamAdapter.h"
#include <cstring>
#include <stdexcept>

namespace OEMaestro {

OEStreamBuf::OEStreamBuf(OEPlatform::oeifstream& ifs, size_t buf_size)
    : ifs_(ifs), buffer_(buf_size) {
    setg(buffer_.data(), buffer_.data(), buffer_.data());
}

OEStreamBuf::int_type OEStreamBuf::underflow() {
    if (gptr() < egptr())
        return traits_type::to_int_type(*gptr());

    // Read from oeifstream into our buffer
    oesize_t bytes_read = ifs_.read(buffer_.data(), static_cast<oesize_t>(buffer_.size()));

    if (bytes_read == 0)
        return traits_type::eof();

    setg(buffer_.data(), buffer_.data(), buffer_.data() + bytes_read);
    return traits_type::to_int_type(*gptr());
}

StreamAdapter::StreamAdapter(OEPlatform::oeifstream& ifs)
    : std::istream(&buf_), buf_(ifs) {}

std::shared_ptr<std::istream> make_maeparser_stream(OEPlatform::oeifstream& ifs) {
    return std::make_shared<StreamAdapter>(ifs);
}

// --- Output adapter ---

OEOutputStreamBuf::OEOutputStreamBuf(OEPlatform::oeofstream& ofs, std::size_t buf_size)
    : ofs_(ofs), buffer_(buf_size + 1) {
    setp(buffer_.data(), buffer_.data() + buf_size);
}

bool OEOutputStreamBuf::FlushBuffer() {
    std::ptrdiff_t n = pptr() - pbase();
    if (n > 0) {
        ofs_.write(pbase(), static_cast<oesize_t>(n));
    }
    pbump(static_cast<int>(-n));
    return true;
}

int OEOutputStreamBuf::overflow(int ch) {
    if (!FlushBuffer()) return traits_type::eof();  // NOLINT -- defensive; FlushBuffer may fail in subclasses
    if (ch != traits_type::eof()) {
        *pptr() = static_cast<char>(ch);
        pbump(1);
    }
    return ch;
}

std::streamsize OEOutputStreamBuf::xsputn(const char* s, std::streamsize count) {
    if (count > epptr() - pptr()) {
        FlushBuffer();
        ofs_.write(s, static_cast<oesize_t>(count));
        return count;
    }
    std::memcpy(pptr(), s, static_cast<std::size_t>(count));
    pbump(static_cast<int>(count));
    return count;
}

int OEOutputStreamBuf::sync() {
    return FlushBuffer() ? 0 : -1;  // NOLINT -- defensive; FlushBuffer may fail in subclasses
}

OutputStreamAdapter::OutputStreamAdapter(OEPlatform::oeofstream& ofs)
    : std::ostream(&buf_), buf_(ofs) {}

std::shared_ptr<std::ostream> make_maeparser_ostream(OEPlatform::oeofstream& ofs) {
    return std::make_shared<OutputStreamAdapter>(ofs);
}

// --- Gzip stream adapters using zlib ---

GzipInputBuf::GzipInputBuf(const std::string& filename, size_t buf_size)
    : gz_(gzopen(filename.c_str(), "rb")), buffer_(buf_size) {
    if (!gz_) {
        throw std::runtime_error("Failed to open gzip file for reading: " + filename);
    }
    setg(buffer_.data(), buffer_.data(), buffer_.data());
}

GzipInputBuf::~GzipInputBuf() {
    if (gz_) gzclose(gz_);
}

GzipInputBuf::int_type GzipInputBuf::underflow() {
    if (gptr() < egptr())
        return traits_type::to_int_type(*gptr());
    if (!gz_)
        return traits_type::eof();

    int bytes = gzread(gz_, buffer_.data(), static_cast<unsigned>(buffer_.size()));
    if (bytes <= 0)
        return traits_type::eof();

    setg(buffer_.data(), buffer_.data(), buffer_.data() + bytes);
    return traits_type::to_int_type(*gptr());
}

GzipInputStream::GzipInputStream(const std::string& filename)
    : std::istream(&buf_), buf_(filename) {}

bool is_gzip_filename(const std::string& filename) {
    return (filename.size() >= 7 && filename.substr(filename.size() - 7) == ".mae.gz") ||
           (filename.size() >= 6 && filename.substr(filename.size() - 6) == ".maegz");
}

std::shared_ptr<std::istream> make_gzip_istream(const std::string& filename) {
    return std::make_shared<GzipInputStream>(filename);
}

GzipOutputBuf::GzipOutputBuf(const std::string& filename, size_t buf_size)
    : gz_(gzopen(filename.c_str(), "wb")), buffer_(buf_size + 1) {
    if (!gz_) {
        throw std::runtime_error("Failed to open gzip file for writing: " + filename);
    }
    setp(buffer_.data(), buffer_.data() + buf_size);
}

GzipOutputBuf::~GzipOutputBuf() {
    if (gz_) {
        FlushBuffer();
        gzclose(gz_);
    }
}

bool GzipOutputBuf::FlushBuffer() {
    std::ptrdiff_t n = pptr() - pbase();
    if (n > 0 && gz_) {
        gzwrite(gz_, pbase(), static_cast<unsigned>(n));
    }
    pbump(static_cast<int>(-n));
    return true;
}

int GzipOutputBuf::overflow(int ch) {
    if (!FlushBuffer()) return traits_type::eof();
    if (ch != traits_type::eof()) {
        *pptr() = static_cast<char>(ch);
        pbump(1);
    }
    return ch;
}

std::streamsize GzipOutputBuf::xsputn(const char* s, std::streamsize count) {
    if (count > epptr() - pptr()) {
        FlushBuffer();
        if (gz_) gzwrite(gz_, s, static_cast<unsigned>(count));
        return count;
    }
    std::memcpy(pptr(), s, static_cast<std::size_t>(count));
    pbump(static_cast<int>(count));
    return count;
}

int GzipOutputBuf::sync() {
    return FlushBuffer() ? 0 : -1;
}

GzipOutputStream::GzipOutputStream(const std::string& filename)
    : std::ostream(&buf_), buf_(filename) {}

GzipOutputStream::~GzipOutputStream() {
    flush();
}

std::shared_ptr<std::ostream> make_gzip_ostream(const std::string& filename) {
    return std::make_shared<GzipOutputStream>(filename);
}

}  // namespace OEMaestro