#pragma once

#include <cstdint>
#include <string>

namespace raptor {


enum MessageType : std::uint8_t {
    EVENT,
    TEXT,
    REQUEST,
    RESPONSE,
    FAILURE,
};

struct Message {
    std::string route;
    MessageType type = EVENT;
    std::uint32_t id = 0;
};

}
