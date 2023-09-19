// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QtGlobal>

#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

/** List of cameras that have footage on the given server. */
struct NX_VMS_API ServerFootageData
{
    QnUuid serverGuid;
    std::vector<QnUuid> archivedCameras;

    ServerFootageData() = default;

    ServerFootageData(const QnUuid& serverGuid, std::vector<QnUuid> archivedCameras):
        serverGuid(serverGuid),
        archivedCameras(std::move(archivedCameras))
    {
    }
};
#define ServerFootageData_Fields (serverGuid)(archivedCameras)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(ServerFootageData)

/**
 * History item of camera movement from server to server. Server and timestamp when the camera
 * moved to it.
 */
struct NX_VMS_API CameraHistoryItemData
{
    CameraHistoryItemData() = default;

    CameraHistoryItemData(const QnUuid& serverGuid, qint64 timestampMs):
        serverGuid(serverGuid),
        timestampMs(timestampMs)
    {
    }

    QnUuid serverGuid;
    qint64 timestampMs = 0;
};
#define CameraHistoryItemData_Fields (serverGuid)(timestampMs)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(CameraHistoryItemData)

/** Full history of the movement for the given camera. */
struct NX_VMS_API CameraHistoryData
{
    QnUuid cameraId;
    CameraHistoryItemDataList items;
};
#define CameraHistoryData_Fields (cameraId)(items)
NX_VMS_API_DECLARE_STRUCT_AND_LIST(CameraHistoryData)

} // namespace api
} // namespace vms
} // namespace nx
