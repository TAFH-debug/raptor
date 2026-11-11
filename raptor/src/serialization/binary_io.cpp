#include <binary_io.h>

namespace raptor {

void write_le(std::ostream& st, std::uint64_t value, std::size_t width) {
    char buf[8];
    for (std::size_t i = 0; i < width; ++i)
        buf[i] = static_cast<char>((value >> (8 * i)) & 0xFF);
        
    st.write(buf, static_cast<std::streamsize>(width));
}

std::uint64_t read_le(std::istream& st, std::size_t width) {
    unsigned char buf[8];

    if (!st.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(width)))
        throw std::runtime_error(std::string("RaptorObject: unexpected end of stream"));

    std::uint64_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= std::uint64_t{buf[i]} << (8 * i);

    return value;
}

void write_string(std::ostream& st, const std::string& s) {
    if (s.size() > UINT32_MAX) throw std::runtime_error(std::string("string too long"));;
    write_le(st, s.size(), kLenWidth);
    st.write(s.data(), static_cast<std::streamsize>(s.size()));
}

std::string read_string(std::istream& st) {
    auto len = static_cast<std::size_t>(read_le(st, kLenWidth));
    std::string s(len, '\0');
    if (!st.read(s.data(), static_cast<std::streamsize>(len)))
        throw std::runtime_error(std::string("unexpected end of stream"));
    return s;
}

}