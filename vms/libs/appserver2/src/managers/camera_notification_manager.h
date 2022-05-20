// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <transaction/transaction.h>

#include <nx_ec/managers/abstract_camera_manager.h>

namespace ec2 {

class QnCameraNotificationManager: public AbstractCameraNotificationManager
{
public:
    QnCameraNotificationManager();

    void triggerNotification(
        const QnTransaction<nx::vms::api::CameraData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::CameraDataList>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::CameraAttributesData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::CameraAttributesDataList>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::IdData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::ServerFootageData>& tran,
        NotificationSource source);
    void triggerNotification(
        const QnTransaction<nx::vms::api::HardwareIdMapping>& tran,
        NotificationSource source);
};

typedef std::shared_ptr<QnCameraNotificationManager> QnCameraNotificationManagerPtr;

} // namespace ec2
