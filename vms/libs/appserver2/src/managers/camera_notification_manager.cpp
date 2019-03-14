#include "camera_notification_manager.h"

namespace ec2
{

QnCameraNotificationManager::QnCameraNotificationManager()
{
}

void QnCameraNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::CameraData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveCamera);
    emit addedOrUpdated(tran.params, source);
}

void QnCameraNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::CameraDataList>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveCameras);
    for (const auto& camera: tran.params)
        emit addedOrUpdated(camera, source);
}

void QnCameraNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::CameraAttributesData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::saveCameraUserAttributes);
    emit userAttributesChanged(tran.params);
}

void QnCameraNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::CameraAttributesDataList>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::saveCameraUserAttributesList);
    for (const auto& attrs: tran.params)
        emit userAttributesChanged(attrs);
}

void QnCameraNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeCamera ||
        tran.command == ApiCommand::removeCameraUserAttributes);
    switch (tran.command)
    {
    case ApiCommand::removeCamera:
        emit removed(tran.params.id);
        break;
    case ApiCommand::removeCameraUserAttributes:
        emit userAttributesRemoved(tran.params.id);
        break;
    default:
        NX_ASSERT(0);
    }
}

void QnCameraNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::ServerFootageData>& tran,
    NotificationSource /*source*/)
{
    if (tran.command == ApiCommand::addCameraHistoryItem)
        emit cameraHistoryChanged(tran.params);
}

} // namespace ec2
