#pragma once

#include <string>

namespace raptor {

enum MessageType {
    EVENT,
    TEXT,
    REQUEST,
    RESPONSE
};

struct Message {
    std::string route;
    MessageType type;
};

}
