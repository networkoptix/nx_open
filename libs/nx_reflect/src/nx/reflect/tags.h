// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::reflect {

/**
 * The useStringConversionForSerialization tag enables use of toString() / fromString() when
 * serializing/deserializing a type. Disabled by default.
 *
 * E.g., unconditional use of toString() on members while serializing a type to JSON may be
 * unexpected if some type wasn't instrumented, but toString() is present for logging/debugging.
 * It is more clear to produce a compile error in this case so that the developer may decide
 * how they want their type to be serialized to JSON.
 */
template<typename T>
static constexpr bool useStringConversionForSerialization(const T*) { return false; }

} // namespace nx::reflect
