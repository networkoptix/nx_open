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

const QString StorageData::kStaticTypeName = lit("Storage");
const QnUuid StorageData::kStaticTypeId =
    ResourceData::typeIdFromName(StorageData::kStaticTypeName);

const QString MediaServerData::kStaticTypeName = lit("Server");
const QnUuid MediaServerData::kStaticTypeId =
    ResourceData::typeIdFromName(MediaServerData::kStaticTypeName);

const int MediaServerUserAttributesData::kDefaultBackupBitrate = kDefaultNotCappedBytesPerSecond;

const DeprecatedFieldNames* MediaServerUserAttributesData::getDeprecatedFieldNames()
{
    static DeprecatedFieldNames kDeprecatedFieldNames{
        { lit("serverId"), lit("serverID") }, //< up to v2.6
    };

    return &kDeprecatedFieldNames;
}

MediaServerDataEx::MediaServerDataEx(const MediaServerDataEx& mediaServerData) :
    MediaServerData(mediaServerData)
{
}

MediaServerDataEx::MediaServerDataEx(MediaServerData&& mediaServerData) :
    MediaServerData(std::move(mediaServerData))
{
}

MediaServerDataEx::MediaServerDataEx(MediaServerDataEx&& mediaServerData) :
    MediaServerData(std::move(mediaServerData)),
    MediaServerUserAttributesData(std::move(mediaServerData)),
    status(mediaServerData.status),
    addParams(std::move(mediaServerData.addParams)),
    storages(std::move(mediaServerData.storages))
{
}

MediaServerDataEx& MediaServerDataEx::operator=(MediaServerDataEx&& mediaServerData)
{
    static_cast<MediaServerData&>(*this) =
        std::move(static_cast<MediaServerData&&>(mediaServerData));
    static_cast<MediaServerUserAttributesData&>(*this) =
        std::move(static_cast<MediaServerUserAttributesData&&>(mediaServerData));

    status = mediaServerData.status;
    addParams = std::move(mediaServerData.addParams);
    storages = std::move(mediaServerData.storages);
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
