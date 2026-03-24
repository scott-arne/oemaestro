#include "oemaestro/StreamAdapter.h"

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

}  // namespace OEMaestro
