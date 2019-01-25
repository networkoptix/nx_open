#pragma once

#include <string>
#include <ostream>

#include <nx/sdk/error.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/i_device_info.h>

namespace nx {
namespace sdk {

/**
 * @return Possibly multiline JSON string without the trailing `\n`:
 * <code><pre>
 *     [
 *         { "name": "...", "value": "..." },
 *         ...
 *     ]
 * </pre></code>
 */
std::string toJsonString(const IStringMap* map, int overallIndent = 0);

/** @return Multiline JSON string without the trailing `\n`. */
std::string toJsonString(const IDeviceInfo* deviceInfo, int overallIndent = 0);

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

//-------------------------------------------------------------------------------------------------
// Functions that need to be in namespace std for compatibility with STL features.

namespace std {

inline std::ostream& operator<<(std::ostream& os, nx::sdk::Error error)
{
    return os << toString(error);
}

} // namespace std
