// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::reflect::json {

/**
 * The default overload for the `jsonSerializeChronoDurationAsNumber` tag.
 */
template<typename T>
static constexpr bool jsonSerializeChronoDurationAsNumber(const T*)
{
    return false;
}

} // namespace nx::reflect::json
