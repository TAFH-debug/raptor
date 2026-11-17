#pragma once

#include <iostream>

namespace raptor {
    class Serializable {
    public:
        virtual ~Serializable() = default;

        virtual void write_binary(std::ostream& st) const = 0;
        virtual void read_binary(std::istream& st) = 0;

    protected:
        Serializable() = default;
        Serializable(const Serializable&) = default;
        Serializable(Serializable&&) = default;
        Serializable& operator=(const Serializable&) = default;
        Serializable& operator=(Serializable&&) = default;
    };
}
