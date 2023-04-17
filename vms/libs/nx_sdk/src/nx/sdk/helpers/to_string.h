// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ostream>
#include <string>

#include <nx/sdk/i_device_info.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/result.h>

namespace nx::sdk {

std::string toString(const IString* string);

/**
 * @return Possibly multiline string with a trailing `\n`, or an empty string if the map is null or
 *     empty:
 * <code><pre>
 *     [name0]: value0
 *     [name1]: value1
 *     ...
 * </pre></code>
 */
std::string toString(const IStringMap* map, int overallIndent = 0);

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

std::string toString(ErrorCode errorCode);

} // namespace nx::sdk

//-------------------------------------------------------------------------------------------------
// Functions that need to be in namespace std for compatibility with STL features.

namespace std {

inline std::ostream& operator<<(std::ostream& os, nx::sdk::ErrorCode errorCode)
{
    return os << nx::sdk::toString(errorCode);
}

} // namespace std
