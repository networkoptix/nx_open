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
struct NX_VMS_API BackupPosition: ServerAndDeviceIdData
{
    /**%apidoc[opt] */
    std::chrono::system_clock::time_point positionLowMs = kDefaultBackupPosition;

    /**%apidoc[opt] */
    std::chrono::system_clock::time_point positionHighMs = kDefaultBackupPosition;

    /**%apidoc[opt] */
    std::chrono::system_clock::time_point bookmarkStartPositionMs = kDefaultBackupPosition;

    const ServerAndDeviceIdData& getId() const { return *this; }
    void setId(ServerAndDeviceIdData id) { static_cast<ServerAndDeviceIdData&>(*this) = std::move(id); }

    bool operator==(const BackupPosition& other) const = default;
};

#define BackupPosition_Fields \
    ServerAndDeviceIdData_Fields \
    (positionLowMs) \
    (positionHighMs) \
    (bookmarkStartPositionMs)

QN_FUSION_DECLARE_FUNCTIONS(BackupPosition, (json)(ubjson), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(BackupPosition, BackupPosition_Fields)

struct NX_VMS_API BackupPositionEx: BackupPosition
{
    BackupPositionEx() = default;
    BackupPositionEx(const BackupPositionEx&) = default;
    BackupPositionEx(const BackupPosition& other);
    std::chrono::milliseconds toBackupLowMs{0};
    std::chrono::milliseconds toBackupHighMs{0};

    bool operator==(const BackupPositionEx& other) const = default;
};

using BackupPositionList = std::vector<BackupPosition>;
using BackupPositionExList = std::vector<BackupPositionEx>;

#define BackupPositionEx_Fields \
    BackupPosition_Fields \
    (toBackupLowMs) \
    (toBackupHighMs)

QN_FUSION_DECLARE_FUNCTIONS(BackupPositionEx, (json)(ubjson), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(BackupPositionEx, BackupPositionEx_Fields)

} // namespace nx::vms::api
