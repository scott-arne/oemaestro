#include "oemaestro/StreamAdapter.h"
#include <cstring>

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
    return std::shared_ptr<std::istream>(new StreamAdapter(ifs));
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
    if (!FlushBuffer()) return traits_type::eof();
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
    return FlushBuffer() ? 0 : -1;
}

OutputStreamAdapter::OutputStreamAdapter(OEPlatform::oeofstream& ofs)
    : std::ostream(&buf_), buf_(ofs) {}

std::shared_ptr<std::ostream> make_maeparser_ostream(OEPlatform::oeofstream& ofs) {
    return std::shared_ptr<std::ostream>(new OutputStreamAdapter(ofs));
}

}  // namespace OEMaestro
