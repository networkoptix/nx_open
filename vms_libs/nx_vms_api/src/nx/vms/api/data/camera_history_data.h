#pragma once

#include "data.h"

#include <vector>

#include <QtCore/QtGlobal>

#include <nx/utils/uuid.h>

namespace nx {
namespace vms {
namespace api {

/** List of cameras that have footage on the given server. */
struct NX_VMS_API ServerFootageData: Data
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

/**
 * History item of camera movement from server to server. Server and timestamp when the camera
 * moved to it.
 */
struct NX_VMS_API CameraHistoryItemData: Data
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

/** Full history of the movement for the given camera. */
struct NX_VMS_API CameraHistoryData: Data
{
    QnUuid cameraId;
    CameraHistoryItemDataList items;
};
#define CameraHistoryData_Fields (cameraId)(items)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::ServerFootageData)
Q_DECLARE_METATYPE(nx::vms::api::ServerFootageDataList)
Q_DECLARE_METATYPE(nx::vms::api::CameraHistoryItemData)
Q_DECLARE_METATYPE(nx::vms::api::CameraHistoryItemDataList)
Q_DECLARE_METATYPE(nx::vms::api::CameraHistoryData)
Q_DECLARE_METATYPE(nx::vms::api::CameraHistoryDataList)
