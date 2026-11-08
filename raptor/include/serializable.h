#pragma once

#include <iostream>

class Serializable {
public:
    virtual void write_binary(std::ostream& st) const = 0;
    virtual void read_binary(std::istream& st) = 0;
};

