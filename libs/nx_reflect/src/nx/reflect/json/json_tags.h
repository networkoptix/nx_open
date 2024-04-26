// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::reflect::json {

/**
 * When set, std::chrono::duration types are serialized as a numeric value into a JSON document.
 * By default, durations are serialized as a string holding the numeric representation.
 */
template<typename T>
static constexpr bool jsonSerializeChronoDurationAsNumber(const T*)
{
    return false;
}

/**
 * When set, 64 bit integer types are serialized into a JSON document as a string holding the
 * numeric representation. By default, 64 bit integers are serialized as a numeric value.
 */
template<typename T>
static constexpr bool jsonSerializeInt64AsString(const T*)
{
    return false;
}

} // namespace nx::reflect::json
