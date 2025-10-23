// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <expected>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>

#include <nx/reflect/json.h>

namespace nx::shared_context_storage {

class NX_SHARED_CONTEXT_STORAGE_API SharedContextStorage
{
public:
    SharedContextStorage() = default;
    ~SharedContextStorage() = default;

    template<typename DataType>
    void writeData(const std::string& id, const std::string& name, const DataType& value)
    {
        if constexpr (!std::is_same_v<DataType, std::string>)
            setValue(makeKey(id, name), nx::reflect::json::serialize(value));
        else
            setValue(makeKey(id, name), std::string(value));
    }

    template<typename DataType>
        requires std::is_move_constructible_v<DataType>
    static std::expected<DataType, std::string> deserializeData(
        const std::string& rawValue)
    {
        if constexpr (std::is_same_v<DataType, std::string>)
            return rawValue;

        auto [deserializedValue, deserializationResult] =
            nx::reflect::json::deserialize<DataType>(rawValue);

        if (deserializationResult.success)
            return deserializedValue;

        return std::unexpected(deserializationResult.toString());
    }

    template<typename DataType>
        requires std::is_move_constructible_v<DataType>
    std::expected<DataType, std::string> readData(
        const std::string& id, const std::string& name) const
    {
        std::string rawValue;
        if (!getValue(makeKey(id, name), &rawValue))
        {
            return std::unexpected(
                "Shared context storage has no value for id \"" + id + "\", name \"" + name + "\"");
        }

        return deserializeData<DataType>(rawValue);
    }

    bool deleteData(const std::string& id, const std::string& name);

private:
    void setValue(std::string&& key, std::string&& value);
    bool getValue(const std::string& key, std::string* value) const;

    static std::string makeKey(const std::string& id, const std::string& name);

private:
    mutable std::mutex m_storageMutex;
    std::unordered_map<std::string, std::string> m_storage;
};

} // namespace nx::shared_context_storage
