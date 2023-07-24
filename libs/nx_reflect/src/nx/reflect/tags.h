// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::reflect {

/**
 * The useStringConversionForSerialization tag enables use of toString() / fromString() when
 * serializing/deserializing a type. Disabled by default.
 */
template<typename T>
static constexpr bool useStringConversionForSerialization(const T*) { return false; }

} // namespace nx::reflect
