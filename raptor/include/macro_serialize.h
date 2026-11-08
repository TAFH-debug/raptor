#pragma once

#include <serializable.h>
#include <binary_io.h>
#include <macro_cycle.h>

#define WRITE(a) raptor::write(st, a);
#define READ(a) raptor::read(st, a);

#define DEFINE_BINARY_SERIALIZE(...)                \
    void write_binary(std::ostream& st) const {     \
        FOR_EACH(WRITE, __VA_ARGS__)                \
    }                                               \
                                                    \
    void read_binary(std::istream& st) {            \
        FOR_EACH(READ, __VA_ARGS__)                 \
    }

#define RAPTOR_CLASS(...) DEFINE_BINARY_SERIALIZE(__VA_ARGS__)

