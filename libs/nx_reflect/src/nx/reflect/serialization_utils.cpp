// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "serialization_utils.h"

namespace nx::reflect {

DeserializationResult::DeserializationResult(bool result):
    success(result), errorDescription(), firstBadFragment()
{
}

DeserializationResult::DeserializationResult(
    bool result,
    std::string errorDescription,
    std::string badFragment,
    std::optional<std::string> notDeserializedField /* = std::nullopt */)
    :
    success(result),
    errorDescription(errorDescription),
    firstBadFragment(badFragment),
    firstNonDeserializedField(notDeserializedField)
{
}

DeserializationResult::operator bool() const noexcept
{
    return success;
}

} // namespace nx::reflect
