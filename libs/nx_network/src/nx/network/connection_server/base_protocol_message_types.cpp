#include "base_protocol_message_types.h"

namespace nx::network::server {

const char* toString(ParserState state)
{
    switch (state)
    {
        case ParserState::init:
            return "init";
        case ParserState::readingMessage:
            return "readingMessage";
        case ParserState::readingBody:
            return "readingBody";
        case ParserState::done:
            return "done";
        case ParserState::failed:
            return "failed";
    }

    return "unknown";
}

} // namespace nx::network::server
