// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/flags.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/server_and_device_id_data.h>

namespace nx::vms::api {

constexpr auto kDefaultBackupPosition =
    std::chrono::system_clock::time_point(std::chrono::milliseconds::zero());

constexpr auto kBackupFailurePosition =
    std::chrono::system_clock::time_point(std::chrono::milliseconds{-2});

struct NX_VMS_API BackupPositionV1: ServerAndDeviceIdData
{
    /**%apidoc[opt] */
    std::chrono::system_clock::time_point positionLowMs = kDefaultBackupPosition;

    /**%apidoc[opt] */
    std::chrono::system_clock::time_point positionHighMs = kDefaultBackupPosition;

    /**%apidoc[opt] */
    std::chrono::system_clock::time_point bookmarkStartPositionMs = kDefaultBackupPosition;

    const ServerAndDeviceIdData& getId() const { return *this; }
    void setId(ServerAndDeviceIdData id) { static_cast<ServerAndDeviceIdData&>(*this) = std::move(id); }

    bool operator==(const BackupPositionV1& other) const = default;
};

#define BackupPositionV1_Fields \
    ServerAndDeviceIdData_Fields \
    (positionLowMs) \
    (positionHighMs) \
    (bookmarkStartPositionMs)

QN_FUSION_DECLARE_FUNCTIONS(BackupPositionV1, (json)(ubjson), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(BackupPositionV1, BackupPositionV1_Fields)

struct NX_VMS_API BackupPositionExV1: BackupPositionV1
{
    using base_type = BackupPositionV1;

    BackupPositionExV1() = default;
    BackupPositionExV1(const BackupPositionExV1&) = default;
    std::chrono::milliseconds toBackupLowMs{0};
    std::chrono::milliseconds toBackupHighMs{0};

    static BackupPositionExV1 fromBase(const BackupPositionV1& other);

    bool operator==(const BackupPositionExV1& other) const = default;
};

#define BackupPositionExV1_Fields \
    BackupPositionV1_Fields \
    (toBackupLowMs) \
    (toBackupHighMs)

QN_FUSION_DECLARE_FUNCTIONS(BackupPositionExV1, (json)(ubjson), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(BackupPositionExV1, BackupPositionExV1_Fields)

using BackupPositionV1List = std::vector<BackupPositionV1>;
using BackupPositionExV1List = std::vector<BackupPositionExV1>;

// V4

/**%apidoc
 * Last processed DB object positions.
 */
struct NX_VMS_API MetadataBackupPosition
{
    /**%apidoc[opt] */
    std::chrono::system_clock::time_point bookmarkPositionMs = kDefaultBackupPosition;

    /**%apidoc[opt] */
    qint64 bookmarkRecordId = 0;

    /**%apidoc[opt] */
    std::chrono::system_clock::time_point motionPositionMs = kDefaultBackupPosition;

    /**%apidoc[opt] */
    std::chrono::system_clock::time_point analyticsPositionMs = kDefaultBackupPosition;

    bool operator==(const MetadataBackupPosition& other) const = default;
};

#define MetadataBackupPosition_Fields \
    (bookmarkPositionMs) \
    (bookmarkRecordId) \
    (motionPositionMs) \
    (analyticsPositionMs)

QN_FUSION_DECLARE_FUNCTIONS(MetadataBackupPosition, (json)(ubjson), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(MetadataBackupPosition, MetadataBackupPosition_Fields)

struct NX_VMS_API MediaBackupPosition
{
    /**%apidoc[opt]
     * Backup position of the low quality stream.
     */
    std::chrono::system_clock::time_point positionLowMs = kDefaultBackupPosition;

    /**%apidoc[opt]
     * Backup position of the hi quality stream.
     */
    std::chrono::system_clock::time_point positionHighMs = kDefaultBackupPosition;

    /**%apidoc[opt]
     * Backup position of the video associated with bookmarks.
     */
    std::chrono::system_clock::time_point bookmarkStartPositionMs = kDefaultBackupPosition;

    bool operator==(const MediaBackupPosition& other) const = default;
};

#define MediaBackupPosition_Fields \
    (positionLowMs) \
    (positionHighMs) \
    (bookmarkStartPositionMs)

QN_FUSION_DECLARE_FUNCTIONS(MediaBackupPosition, (json)(ubjson), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(MediaBackupPosition, MediaBackupPosition_Fields)

struct NX_VMS_API BackupPositionV4: ServerAndDeviceIdData
{
    MediaBackupPosition media;
    MetadataBackupPosition metadata;

    const ServerAndDeviceIdData& getId() const { return *this; }
    void setId(ServerAndDeviceIdData id) { static_cast<ServerAndDeviceIdData&>(*this) = std::move(id); }

    bool operator==(const BackupPositionV4& other) const = default;
    BackupPositionV1 toV1() const;

    static BackupPositionV4 fromV1(const BackupPositionV1& data);
};

#define BackupPositionV4_Fields \
    ServerAndDeviceIdData_Fields \
    (media) \
    (metadata)

QN_FUSION_DECLARE_FUNCTIONS(BackupPositionV4, (json)(ubjson), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(BackupPositionV4, BackupPositionV4_Fields)

struct NX_VMS_API BackupPositionExV4: BackupPositionV4
{
    using base_type = BackupPositionV4;

    BackupPositionExV4() = default;
    BackupPositionExV4(const BackupPositionExV4&) = default;
    std::chrono::milliseconds toBackupLowMs{0};
    std::chrono::milliseconds toBackupHighMs{0};

    static BackupPositionExV4 fromBase(const BackupPositionV4& other);

    bool operator==(const BackupPositionExV4& other) const = default;
};

#define BackupPositionExV4_Fields \
    BackupPositionV4_Fields \
    (toBackupLowMs) \
    (toBackupHighMs)

QN_FUSION_DECLARE_FUNCTIONS(BackupPositionExV4, (json)(ubjson), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(BackupPositionExV4, BackupPositionExV4_Fields)

using BackupPositionV4List = std::vector<BackupPositionV4>;
using BackupPositionExV4List = std::vector<BackupPositionExV4>;

using BackupPosition = BackupPositionV4;
using BackupPositionEx = BackupPositionExV4;
using BackupPositionList = BackupPositionV4List;
using BackupPositionExList = BackupPositionExV4List;

} // namespace nx::vms::api
