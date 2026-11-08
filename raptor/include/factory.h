#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <serializable.h>

class Factory {
public:
    using Creator = std::function<std::unique_ptr<Serializable>()>;

    static Factory& instance() { static Factory f; return f; }

    void add(const std::string& name, Creator c) { creators[name] = std::move(c); }

    std::unique_ptr<Serializable> create(const std::string& name) const {
        auto it = creators.find(name);
        if (it == creators.end()) throw std::runtime_error("Unknown type: " + name);
        return it->second();
    }
private:
    std::unordered_map<std::string, Creator> creators;
};

template <typename T>
struct Register {
    explicit Register(const std::string& name) {
        Factory::instance().add(name, [] { return std::make_unique<T>(); });
    }
};
