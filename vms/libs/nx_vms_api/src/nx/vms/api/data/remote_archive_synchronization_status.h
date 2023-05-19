// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <vector>

#include <nx/reflect/enum_instrument.h>
#include <nx/vms/api/data/server_and_device_id_data.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(RemoteArchiveSynchronizationStateCode,
    idle, /**<%apidoc Stopped because recording (but not import) has been disabled. */
    inProgress,
    disabled,
    error
);

struct NX_VMS_API RemoteArchiveSynchronizationStatus: ServerAndDeviceIdData
{
    RemoteArchiveSynchronizationStateCode code = RemoteArchiveSynchronizationStateCode::idle;
    /**%apidoc A Unix timestamp (milliseconds since epoch) of the current import position. */
    std::chrono::system_clock::time_point importedPositionMs{std::chrono::milliseconds(0)};
    /**%apidoc The duration of the archive left to import from the Device. */
    std::chrono::milliseconds durationToImportMs{std::chrono::milliseconds(0)};

    const ServerAndDeviceIdData& getId() const { return *this; }
    void setId(ServerAndDeviceIdData id) { static_cast<ServerAndDeviceIdData&>(*this) = std::move(id); }

    bool operator==(const RemoteArchiveSynchronizationStatus& other) const = default;
};
#define RemoteArchiveSynchronizationStatus_Fields \
    ServerAndDeviceIdData_Fields \
    (code) \
    (importedPositionMs) \
    (durationToImportMs)

QN_FUSION_DECLARE_FUNCTIONS(RemoteArchiveSynchronizationStatus, (json)(ubjson), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(RemoteArchiveSynchronizationStatus,
    RemoteArchiveSynchronizationStatus_Fields)

using RemoteArchiveSynchronizationStatusList = std::vector<RemoteArchiveSynchronizationStatus>;

} // namespace nx::vms::api
