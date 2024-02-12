// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_resource_index.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

CameraResourceIndex::CameraResourceIndex(const QnResourcePool* resourcePool):
    base_type(),
    m_resourcePool(resourcePool)
{
    if (!(NX_ASSERT(resourcePool)))
        return;

    blockSignals(true);
    indexAllCameras();
    blockSignals(false);

    connect(m_resourcePool, &QnResourcePool::resourceAdded,
        this, &CameraResourceIndex::onResourceAdded);

    connect(m_resourcePool, &QnResourcePool::resourceRemoved,
        this, &CameraResourceIndex::onResourceRemoved);
}

QVector<QnResourcePtr> CameraResourceIndex::allCameras() const
{
    QVector<QnResourcePtr> result;
    for (const auto& camerasSet: m_camerasByServer)
        std::copy(camerasSet.cbegin(), camerasSet.cend(), std::back_inserter(result));
    return result;
}

QVector<QnResourcePtr> CameraResourceIndex::camerasOnServer(
    const QnResourcePtr& server) const
{
    const auto& camerasSet = m_camerasByServer.value(server->getId());
    return QVector<QnResourcePtr>(camerasSet.cbegin(), camerasSet.cend());
}

void CameraResourceIndex::indexAllCameras()
{
    const QnVirtualCameraResourceList cameras =
        m_resourcePool->getAllCameras(QnResourcePtr(), /*ignoreDesktopCameras*/ true);

    for (const auto& camera: cameras)
        indexCamera(camera);
}

void CameraResourceIndex::onResourceAdded(const QnResourcePtr& resource)
{
    if (resource->hasFlags(Qn::desktop_camera))
        return;

    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        indexCamera(camera);
}

void CameraResourceIndex::onResourceRemoved(const QnResourcePtr& resource)
{
    if (resource->hasFlags(Qn::desktop_camera))
        return;

    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        m_camerasByServer[camera->getParentId()].remove(resource);
        emit cameraRemoved(camera);
        emit cameraRemovedFromServer(camera, camera->getParentResource());
    }
}

void CameraResourceIndex::onCameraParentIdChanged(
    const QnResourcePtr& resource,
    const nx::Uuid& previousParentServerId)
{
    const auto camera = resource.staticCast<QnVirtualCameraResource>();

    const nx::Uuid parentServerId = camera->getParentId();
    const auto previousParentServer = m_resourcePool->getResourceById(previousParentServerId);

    m_camerasByServer[previousParentServerId].remove(camera);
    emit cameraRemovedFromServer(camera, previousParentServer);

    m_camerasByServer[parentServerId].insert(camera);
    emit cameraAddedToServer(camera, camera->getParentResource());
}

void CameraResourceIndex::indexCamera(const QnVirtualCameraResourcePtr& camera)
{
    m_camerasByServer[camera->getParentId()].insert(camera);

    connect(camera.get(), &QnVirtualCameraResource::parentIdChanged,
        this, &CameraResourceIndex::onCameraParentIdChanged);

    emit cameraAdded(camera);
    emit cameraAddedToServer(camera, camera->getParentResource());
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
