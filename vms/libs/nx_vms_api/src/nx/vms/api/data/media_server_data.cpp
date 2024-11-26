// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_server_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/reflect/json.h>

namespace nx {
namespace vms {
namespace api {

namespace {

static constexpr int kBitsPerMegabit = 1'000'000;
static constexpr int kBitsPerByte = 8;

const auto kKeyAttributeName = "key";
const auto kValueAttributeName = "value";

} // namespace

const QString StorageData::kResourceTypeName = lit("Storage");
const nx::Uuid StorageData::kResourceTypeId =
    ResourceData::getFixedTypeId(StorageData::kResourceTypeName);

const QString MediaServerData::kResourceTypeName = lit("Server");
const nx::Uuid MediaServerData::kResourceTypeId =
    ResourceData::getFixedTypeId(MediaServerData::kResourceTypeName);

nx::reflect::DeserializationResult deserialize(
    const nx::reflect::json::DeserializationContext& context,
    BackupBitrateBytesPerSecond* value)
{
    using namespace nx::reflect;

    if (!context.value.IsArray())
    {
        return {false, "Invalid BackupBitratePerSecond format",
            nx::reflect::json_detail::getStringRepresentation(context.value)};
    }

    for (int i = 0; i < (int) context.value.Size(); ++i)
    {
        const auto& item = context.value[i];
        if (!item.IsObject())
        {
            return {false, "Invalid BackupBitratePerSecond format",
                nx::reflect::json_detail::getStringRepresentation(context.value)};
        }

        BackupBitrateKey elemKey;
        qint64 elemValue = 0;

        if (auto it = item.FindMember(kKeyAttributeName); it != item.MemberEnd())
        {
            if (auto result = nx::reflect::json::deserialize(
                nx::reflect::json::DeserializationContext{it->value}, &elemKey);
                !result)
            {
                return result;
            }
        }

        if (auto it = item.FindMember(kValueAttributeName); it != item.MemberEnd())
        {
            if (auto result = nx::reflect::json::deserialize(
                nx::reflect::json::DeserializationContext{it->value}, &elemValue);
                !result)
            {
                return result;
            }
        }

        value->insert(std::move(elemKey), std::move(elemValue));
    }

    return DeserializationResult(true);
}

const DeprecatedFieldNames* MediaServerUserAttributesData::getDeprecatedFieldNames()
{
    static DeprecatedFieldNames kDeprecatedFieldNames{
        { lit("serverId"), lit("serverID") }, //< up to v2.6
    };

    return &kDeprecatedFieldNames;
}

MediaServerDataEx::MediaServerDataEx(const MediaServerData& slice):
    MediaServerData(slice)
{
}

MediaServerDataEx::MediaServerDataEx(MediaServerData&& slice):
    MediaServerData(std::move(slice))
{
}

MediaServerDataEx& MediaServerDataEx::operator=(const MediaServerData& slice)
{
    static_cast<MediaServerData&>(*this) = slice;
    return *this;
}

MediaServerDataEx& MediaServerDataEx::operator=(MediaServerData&& slice)
{
    static_cast<MediaServerData&>(*this) = std::move(slice);
    return *this;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BackupBitrateKey, (ubjson)(xml)(json)(sql_record)(csv_record), BackupBitrateKey_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    StorageData, (ubjson)(xml)(json)(sql_record)(csv_record), StorageData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    MediaServerData, (ubjson)(xml)(json)(sql_record)(csv_record), MediaServerData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MediaServerUserAttributesData,
    (ubjson)(xml)(json)(sql_record)(csv_record), MediaServerUserAttributesData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    MediaServerDataEx, (ubjson)(xml)(json)(sql_record)(csv_record), MediaServerDataEx_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(StorageFilter, (csv_record)(json)(ubjson)(xml), (id)(serverId))

} // namespace api
} // namespace vms
} // namespace nx
