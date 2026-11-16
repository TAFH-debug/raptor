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
    if (s.find('\0') != std::string::npos)
        throw std::runtime_error(std::string("RaptorError: string contains '\\0'"));
    st.write(s.data(), static_cast<std::streamsize>(s.size()));
    st.put('\0');
}

std::string read_string(std::istream& st) {
    std::string s;
    // eof means the stream ended before the terminator was found.
    if (!std::getline(st, s, '\0') || st.eof())
        throw std::runtime_error(std::string("RaptorError: unterminated string"));
    return s;
}

}