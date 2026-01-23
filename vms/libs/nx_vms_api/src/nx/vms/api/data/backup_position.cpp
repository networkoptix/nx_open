// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_position.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

BackupPositionExV1 BackupPositionExV1::fromBase(const BackupPositionV1& other)
{
    BackupPositionExV1 result;
    static_cast<BackupPositionV1&>(result) = other;
    return result;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BackupPositionV1, (json)(ubjson), BackupPositionV1_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BackupPositionExV1, (json)(ubjson), BackupPositionExV1_Fields)

// V4
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MetadataBackupPosition, (json)(ubjson), MetadataBackupPosition_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(MediaBackupPosition, (json)(ubjson), MediaBackupPosition_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BackupPositionV4, (json)(ubjson), BackupPositionV4_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(BackupPositionExV4, (json)(ubjson), BackupPositionExV4_Fields)

BackupPositionExV4 BackupPositionExV4::fromBase(const BackupPositionV4& other)
{
    BackupPositionExV4 result;
    static_cast<BackupPositionV4&>(result) = other;
    return result;
}

BackupPositionV1 BackupPositionV4::toV1() const
{
    BackupPositionV1 result;
    result.deviceId = deviceId;
    result.serverId = serverId;
    result.positionLowMs = media.positionLowMs;
    result.positionHighMs = media.positionHighMs;
    result.bookmarkStartPositionMs = media.bookmarkLowStartPositionMs;
    return result;
}

BackupPositionV4 BackupPositionV4::fromV1(const BackupPositionV1& data)
{
    BackupPositionV4 result;
    result.deviceId = data.deviceId;
    result.serverId = data.serverId;

    nx::vms::api::MediaBackupPosition localMedia;
    localMedia.positionLowMs = data.positionLowMs;
    localMedia.positionHighMs = data.positionHighMs;
    localMedia.bookmarkLowStartPositionMs = data.bookmarkStartPositionMs;
    localMedia.bookmarkHighStartPositionMs = data.bookmarkStartPositionMs;

    result.media = localMedia;
    return result;
}

void incrementMediaPosition(MediaBackupPosition& current, const MediaBackupPosition& target)
{
    updateIfGreater(&current.positionLowMs, target.positionLowMs);
    updateIfGreater(&current.positionHighMs, target.positionHighMs);
    updateIfGreater(&current.bookmarkLowStartPositionMs, target.bookmarkLowStartPositionMs);
    updateIfGreater(&current.bookmarkHighStartPositionMs, target.bookmarkHighStartPositionMs);
}

void incrementMetadataPosition(MetadataBackupPosition& current, const MetadataBackupPosition& target)
{
    updateIfGreater(&current.bookmarkPositionMs, target.bookmarkPositionMs);
    updateIfGreater(&current.bookmarkRecordId, target.bookmarkRecordId);
    updateIfGreater(&current.motionPositionMs, target.motionPositionMs);
    updateIfGreater(&current.analyticsPositionMs, target.analyticsPositionMs);
}

MediaBackupPosition BackupPositionV4::getEffectiveMedia(bool useCloud) const
{
    if (useCloud && cloud)
        return *cloud;
    return media;
}

} // namespace nx::vms::api
