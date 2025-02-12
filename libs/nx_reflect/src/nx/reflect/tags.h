// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "has_from_string.h"
#include "has_to_string.h"

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

#define NX_REFLECTION_TAG_TYPE_useStringConversionForSerialization(TAG, TYPE) \
    static constexpr bool TAG(const TYPE*) \
    { \
        static_assert(nx::reflect::detail::HasFromStringV<TYPE> \
                || nx::reflect::detail::HasFromStdStringV<TYPE> \
                || nx::reflect::detail::HasFreeStandFromStringV<TYPE>, \
            "Provide any fromString method"); \
        static_assert(nx::reflect::detail::HasToStringV<TYPE> \
                || nx::reflect::detail::HasToStdStringV<TYPE> \
                || nx::reflect::detail::HasFreeStandToStringV<TYPE>, \
            "Provide any toString method"); \
        return true; \
    }
#define NX_REFLECTION_TAG_TYPE_AS_SECOND_ARG_useStringConversionForSerialization \
    _, NX_REFLECTION_TAG_TYPE_useStringConversionForSerialization
