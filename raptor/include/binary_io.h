#pragma once

#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <serializable.h>

constexpr std::size_t kLenWidth = 4;
constexpr std::size_t kTagWidth = 1;

namespace raptor {
    template <class T>
    constexpr std::size_t wire_width =
        (std::is_same_v<T, long> || std::is_same_v<T, unsigned long>) ? 8 : sizeof(T);

    void write_le(std::ostream& st, std::uint64_t value, std::size_t width);
    std::uint64_t read_le(std::istream& st, std::size_t width);

    template <class T>
    void write_int(std::ostream& st, T value) {
        std::uint64_t bits;
        if constexpr (std::is_signed_v<T>)
            bits = static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
        else
            bits = static_cast<std::uint64_t>(value);
        write_le(st, bits, wire_width<T>);
    }

    template <class T>
    T read_int(std::istream& st) {
        constexpr std::size_t width = wire_width<T>;
        std::uint64_t bits = read_le(st, width);

        if constexpr (std::is_signed_v<T>) {
            if constexpr (width < 8) {
                if ((bits >> (8 * width - 1)) & 1)
                    bits |= ~std::uint64_t{0} << (8 * width); // sign-extend
            }
            auto value = static_cast<std::int64_t>(bits);
            if (!std::in_range<T>(value)) throw std::runtime_error(std::string("RaptorObject: integer out of range"));
            return static_cast<T>(value);
        } else {
            if (!std::in_range<T>(bits)) throw std::runtime_error(std::string("RaptorObject: integer out of range"));
            return static_cast<T>(bits);
        }
    }

    void write_string(std::ostream& st, const std::string& s);

    std::string read_string(std::istream& st);

    template <class T>
    void write(std::ostream& st, const T& v) {
        if constexpr (std::is_base_of_v<Serializable, T>) v.write_binary(st);
        else if constexpr (std::is_same_v<T, std::string>) write_string(st, v);
        else write_int(st, v);
    }

    template <class T>
    void read(std::istream& st, T& v) {
        if constexpr (std::is_base_of_v<Serializable, T>) v.read_binary(st);
        else if constexpr (std::is_same_v<T, std::string>) v = read_string(st);
        else v = read_int<T>(st);
    }
}