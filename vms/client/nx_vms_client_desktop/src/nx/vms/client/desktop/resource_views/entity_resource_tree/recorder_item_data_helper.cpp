// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recorder_item_data_helper.h"

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>

#include <nx/vms/client/desktop/resource_views/entity_resource_tree/camera_resource_index.h>
#include <nx/vms/common/system_context.h>

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
    for (const auto& camera: camerasSet)
    {
        const auto userDefinedGroupName = camera->getUserDefinedGroupName();
        if (!userDefinedGroupName.isEmpty())
            return userDefinedGroupName;
    }
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

bool RecorderItemDataHelper::hasPermissions(
    const QString& recorderGroupId,
    const QnUserResourcePtr& user,
    Qn::Permissions permissions) const
{
    if (user.isNull())
        return false;

    const auto& camerasSet = m_camerasByRecorderGroupId.value(recorderGroupId);

    return std::any_of(camerasSet.begin(), camerasSet.end(),
        [user, &permissions](auto camera)
        {
            const auto accessManager = user->systemContext()->resourceAccessManager();
            // TODO: Qt6 adds testFlags().
            return (accessManager->permissions(user, camera) & permissions) == permissions;
        });
}

void RecorderItemDataHelper::onCameraAdded(const QnResourcePtr& cameraResource)
{
    const auto camera = cameraResource.staticCast<QnVirtualCameraResource>();
    const QString groupId = camera->getGroupId();
    if (groupId.isEmpty())
        return;

    m_camerasByRecorderGroupId[groupId].insert(camera);

    connect(camera.get(), &QnVirtualCameraResource::groupIdChanged,
        this, &RecorderItemDataHelper::onCameraGroupIdChanged);

    connect(camera.get(), &QnVirtualCameraResource::groupNameChanged,
        this, &RecorderItemDataHelper::onCameraGroupNameChanged);
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
