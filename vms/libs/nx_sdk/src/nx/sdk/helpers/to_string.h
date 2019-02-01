#pragma once

#include <string>

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

/**
 * @return Multiline JSON string without the trailing `\n`.
 */
std::string toJsonString(const IDeviceInfo* deviceInfo, int overallIndent = 0);

} // namespace sdk
} // namespace nx
