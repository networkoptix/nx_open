#include "camera_notification_manager.h"

namespace ec2
{

QnCameraNotificationManager::QnCameraNotificationManager()
{
}

void QnCameraNotificationManager::triggerNotification(
    const QnTransaction<ApiCameraData>& tran, 
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveCamera);
    emit addedOrUpdated(tran.params, source);
}

void QnCameraNotificationManager::triggerNotification(
    const QnTransaction<ApiCameraDataList>& tran, 
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveCameras);
    for (const ApiCameraData& camera : tran.params)
        emit addedOrUpdated(camera, source);
}

void QnCameraNotificationManager::triggerNotification(
    const QnTransaction<ApiCameraAttributesData>& tran, 
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::saveCameraUserAttributes);
    emit userAttributesChanged(tran.params);
}

void QnCameraNotificationManager::triggerNotification(
    const QnTransaction<ApiCameraAttributesDataList>& tran, 
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::saveCameraUserAttributesList);
    for (const ApiCameraAttributesData& attrs : tran.params)
        emit userAttributesChanged(attrs);
}

void QnCameraNotificationManager::triggerNotification(
    const QnTransaction<ApiIdData>& tran, 
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
    const QnTransaction<ApiServerFootageData>& tran, 
    NotificationSource /*source*/)
{
    if (tran.command == ApiCommand::addCameraHistoryItem)
        emit cameraHistoryChanged(tran.params);
}

} // namespace ec2
