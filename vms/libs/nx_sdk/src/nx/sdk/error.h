#pragma once

namespace nx {
namespace sdk {

enum class Error
{
    // ATTENTION: Values match error constants in camera_plugin.h.
    noError = 0,
    unknownError = -100,
    networkError = -22,
};

static inline const char* toString(Error error)
{
    switch (error)
    {
        case Error::noError: return "noError";
        case Error::unknownError: return "unknownError";
        case Error::networkError: return "networkError";
        default: return "<unsupported Error>";
    }
}

} // namespace sdk
} // namespace nx
