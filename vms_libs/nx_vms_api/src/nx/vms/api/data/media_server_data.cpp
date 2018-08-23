#include "media_server_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

namespace {

static constexpr int kBitsPerMegabit = 1000000;
static constexpr int kBitsPerByte = 8;
static constexpr int kDefaultNotCappedBytesPerSecond = -100 * kBitsPerMegabit / kBitsPerByte;

} // namespace

const QString StorageData::kResourceTypeName = lit("Storage");
const QnUuid StorageData::kResourceTypeId =
    ResourceData::getFixedTypeId(StorageData::kResourceTypeName);

const QString MediaServerData::kResourceTypeName = lit("Server");
const QnUuid MediaServerData::kResourceTypeId =
    ResourceData::getFixedTypeId(MediaServerData::kResourceTypeName);

const int MediaServerUserAttributesData::kDefaultBackupBitrate = kDefaultNotCappedBytesPerSecond;

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

#define SERVER_TYPES \
    (StorageData) \
    (MediaServerData) \
    (MediaServerUserAttributesData) \
    (MediaServerDataEx)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    SERVER_TYPES,
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
