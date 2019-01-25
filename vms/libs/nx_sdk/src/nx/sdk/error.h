#pragma once

namespace nx {
namespace sdk {

/**
 * Error codes used both by Plugin methods and Server callbacks.
 *
 * ATTENTION: The values match error constants in <camera/camera_plugin.h>.
 */
enum class Error: int
{
    noError = 0,
    unknownError = -100,
    networkError = -22,
};

inline const char* toString(Error error)
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
