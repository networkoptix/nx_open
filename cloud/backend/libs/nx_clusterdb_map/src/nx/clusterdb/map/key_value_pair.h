#pragma once

#include <string>

#include <nx/network/buffer.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/clusterdb/engine/command_descriptor.h>

namespace nx::clusterdb::map {

struct Key
{
    std::string key;
};

#define Key_Fields (key)

struct KeyValuePair
{
    std::string key;
    std::string value;

    bool operator==(const KeyValuePair& right) const
    {
        return key == right.key && value == right.value;
    }
};

#define KeyValuePair_Fields (key)(value)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (Key)(KeyValuePair),
    (ubjson)(json))

//-------------------------------------------------------------------------------------------------

namespace command {

enum Code
{
    saveKeyValuePair = nx::clusterdb::engine::command::kUserCommand + 1,
    removeKeyValuePair,
};

struct SaveKeyValuePair
{
    using Data = KeyValuePair;
    static constexpr int code = Code::saveKeyValuePair;
    static constexpr char name[] = "saveKeyValuePair";

    static nx::Buffer hash(const Data& data)
    {
        return data.key.c_str();
    }
};

struct RemoveKeyValuePair
{
    using Data = Key;
    static constexpr int code = Code::removeKeyValuePair;
    static constexpr char name[] = "removeKeyValuePair";

    static nx::Buffer hash(const Data& data)
    {
        return data.key.c_str();
    }
};

} // namespace command

} // namespace nx::clusterdb::map
