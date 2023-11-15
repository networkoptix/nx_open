// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>

namespace nx::reflect {

struct NX_REFLECT_API DeserializationResult
{
    bool success = true;
    std::string errorDescription;

    /**
     * A fragment of an input string which wasn't parsed
     */
    std::string firstBadFragment;

    /**
     * A name of the least common ancestor of object type of the field which wasn't deserialized,
     * if exists.
     */
    std::optional<std::string> firstNonDeserializedField;

    DeserializationResult(bool result);
    DeserializationResult() = default;
    DeserializationResult(
        bool result,
        std::string errorDescription,
        std::string badFragment,
        std::optional<std::string> notDeserializedField = std::nullopt);

    operator bool() const noexcept;

    std::string toString() const;
};

} // namespace nx::reflect
