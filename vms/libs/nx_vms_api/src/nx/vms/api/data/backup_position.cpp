// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "backup_position.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

BackupPositionExV1 BackupPositionExV1::fromBase(const BackupPositionV1& other)
{
    BackupPositionExV1 result;
    result.deviceId = other.deviceId;
    result.serverId = other.serverId;
    result.positionLowMs = other.positionLowMs;
    result.positionHighMs = other.positionHighMs;
    result.bookmarkStartPositionMs = other.bookmarkStartPositionMs;
    return result;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BackupPositionV1,
    (json)(ubjson),
    BackupPositionV1_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BackupPositionExV1,
    (json)(ubjson),
    BackupPositionExV1_Fields)

// V4
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    MetadataBackupPosition,
    (json)(ubjson),
    MetadataBackupPosition_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    MediaBackupPosition,
    (json)(ubjson),
    MediaBackupPosition_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BackupPositionV4,
    (json)(ubjson),
    BackupPositionV4_Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    BackupPositionExV4,
    (json)(ubjson),
    BackupPositionExV4_Fields)

BackupPositionExV4 BackupPositionExV4::fromBase(const BackupPositionV4& other)
{
    BackupPositionExV4 result;
    result.deviceId = other.deviceId;
    result.serverId = other.serverId;
    result.media = other.media;
    result.metadata = other.metadata;
    return result;
}

BackupPositionV1 BackupPositionV4::toV1() const
{
    BackupPositionV1 result = BackupPositionV1{
        .positionLowMs = media.positionLowMs,
        .positionHighMs = media.positionHighMs,
        .bookmarkStartPositionMs = media.bookmarkStartPositionMs
    };
    result.deviceId = deviceId;
    result.serverId = serverId;
    return result;
}

BackupPositionV4 BackupPositionV4::fromV1(const BackupPositionV1& data)
{
    BackupPositionV4 result;
    result.deviceId = data.deviceId;
    result.serverId = data.serverId;
    result.media.positionLowMs = data.positionLowMs;
    result.media.positionHighMs = data.positionHighMs;
    result.media.bookmarkStartPositionMs = data.bookmarkStartPositionMs;
    return result;
}

} // namespace nx::vms::api
