// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/sdk/i_utility_provider.h>

#include "shared_context_storage.h"

namespace nx::shared_context_storage {

// This class is needed to export the template functions from the library.
class NX_SHARED_CONTEXT_STORAGE_API Utils
{
public:
    template<typename DataType>
    static std::optional<DataType> readDataUsingUtilityProvider(
        const nx::sdk::Ptr<nx::sdk::IUtilityProvider>& utilityProvider,
        const std::string& id,
        const std::string& name)
    {
        std::optional<std::string> rawValue = utilityProvider->sharedContextValue(
            id.c_str(), name.c_str());

        if (!rawValue)
            return std::nullopt;

        auto deserializedData = SharedContextStorage::deserializeData<DataType>(*rawValue);
        return deserializedData.has_value()
            ? std::optional<DataType>(deserializedData.value())
            : std::nullopt;
    }
};

} // namespace nx::shared_context_storage
