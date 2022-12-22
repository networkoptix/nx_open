// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recorder_item_data_helper.h"

#include <core/resource/camera_resource.h>

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/camera_resource_index.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

RecorderItemDataHelper::RecorderItemDataHelper(
    const CameraResourceIndex* cameraResourceIndex)
    :
    base_type()
{
    const auto allCameras = cameraResourceIndex->allCameras();
    for (const auto& camera: allCameras)
        onCameraAdded(camera);

    connect(cameraResourceIndex, &CameraResourceIndex::cameraAdded, this,
        &RecorderItemDataHelper::onCameraAdded);

    connect(cameraResourceIndex, &CameraResourceIndex::cameraRemoved, this,
        &RecorderItemDataHelper::onCameraRemoved);
}

QString RecorderItemDataHelper::groupedDeviceName(const QString& recorderGroupId) const
{
    const auto& camerasSet = m_camerasByRecorderGroupId.value(recorderGroupId);

    const auto getUserDefinedGroupName =
        [&camerasSet]
        {
            for (const auto& camera: camerasSet)
            {
                // Avoid QnSecurityCamResource::getUserDefinedGroupName() fallback to
                // QnSecurityCamResource::getDefaultGroupName().
                const auto groupName = camera->getUserDefinedGroupName();
                if (!groupName.isEmpty() && groupName != camera->getDefaultGroupName())
                    return groupName;
            }
            return QString();
        };

    const auto getDefaultGroupName =
        [&camerasSet]
        {
            for (const auto& camera: camerasSet)
            {
                if (const auto groupName = camera->getDefaultGroupName(); !groupName.isEmpty())
                    return groupName;
            }
            return QString();
        };

    if (const auto userGroupName = getUserDefinedGroupName(); !userGroupName.isEmpty())
        return userGroupName;
    else if (const auto defaultGroupName = getDefaultGroupName(); !defaultGroupName.isEmpty())
        return defaultGroupName;
    else
        return recorderGroupId;
}

bool RecorderItemDataHelper::isMultisensorCamera(const QString& recorderGroupId) const
{
    const auto& camerasSet = m_camerasByRecorderGroupId.value(recorderGroupId);
    return !camerasSet.empty() && (*camerasSet.cbegin())->isMultiSensorCamera();
}

bool RecorderItemDataHelper::isRecorder(const QString& recorderGroupId) const
{
    const auto& camerasSet = m_camerasByRecorderGroupId.value(recorderGroupId);
    return !camerasSet.empty() && !(*camerasSet.cbegin())->isMultiSensorCamera();
}

void RecorderItemDataHelper::onCameraAdded(const QnResourcePtr& cameraResource)
{
    const auto camera = cameraResource.staticCast<QnVirtualCameraResource>();
    const QString groupId = camera->getGroupId();

    connect(camera.get(), &QnVirtualCameraResource::groupIdChanged,
        this, &RecorderItemDataHelper::onCameraGroupIdChanged);

    connect(camera.get(), &QnVirtualCameraResource::groupNameChanged,
        this, &RecorderItemDataHelper::onCameraGroupNameChanged);

    if (groupId.isEmpty())
        return;

    m_camerasByRecorderGroupId[groupId].insert(camera);
}

void RecorderItemDataHelper::onCameraRemoved(const QnResourcePtr& cameraResource)
{
    const auto camera = cameraResource.staticCast<QnVirtualCameraResource>();
    m_camerasByRecorderGroupId[camera->getGroupId()].remove(camera);
    camera->disconnect(this);
}

void RecorderItemDataHelper::onCameraGroupIdChanged(
    const QnResourcePtr& cameraResource,
    const QString& previousGroupId)
{
    const auto camera = cameraResource.staticCast<QnVirtualCameraResource>();
    const QString groupId = camera->getGroupId();

    if (!previousGroupId.isEmpty())
        m_camerasByRecorderGroupId[previousGroupId].remove(camera);

    if (!groupId.isEmpty())
        m_camerasByRecorderGroupId[groupId].insert(camera);
}

void RecorderItemDataHelper::onCameraGroupNameChanged(const QnResourcePtr& cameraResource)
{
    const auto camera = cameraResource.staticCast<QnVirtualCameraResource>();
    emit groupedDeviceNameChanged(camera->getGroupId());
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
