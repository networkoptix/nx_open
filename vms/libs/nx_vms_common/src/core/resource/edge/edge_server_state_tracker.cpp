// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "edge_server_state_tracker.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <utils/camera/edge_device.h>

namespace nx::core::resource {
namespace edge {

EdgeServerStateTracker::EdgeServerStateTracker(QnMediaServerResource* edgeServer):
    QObject(nullptr),
    m_edgeServer(edgeServer)
{
    if (!NX_ASSERT(m_edgeServer
        && m_edgeServer->resourcePool()
        && !m_edgeServer->hasFlags(Qn::removed)
        && m_edgeServer->getServerFlags().testFlag(nx::vms::api::SF_Edge)))
    {
        return;
    }

    const auto resourcePool = edgeServer->resourcePool();

    connect(resourcePool, &QnResourcePool::resourceAdded,
        this, &EdgeServerStateTracker::onResourceAdded);

    connect(resourcePool, &QnResourcePool::resourceRemoved,
        this, &EdgeServerStateTracker::onResourceRemoved);
}

bool EdgeServerStateTracker::hasUniqueCoupledChildCamera() const
{
    return (m_childCamerasIds & m_coupledCamerasIds).size() == 1;
}

QnVirtualCameraResourcePtr EdgeServerStateTracker::uniqueCoupledChildCamera() const
{
    if (!m_camerasTrackingInitialized)
        return {};

    const auto childCoupledCamerasIds = m_childCamerasIds & m_coupledCamerasIds;
    if (childCoupledCamerasIds.size() == 1)
    {
        return m_edgeServer->resourcePool()->getResourceById(*childCoupledCamerasIds.begin())
            .dynamicCast<QnVirtualCameraResource>();
    }

    return {};
}

bool EdgeServerStateTracker::hasCanonicalState() const
{
    return hasUniqueCoupledChildCamera() && m_childCamerasIds.size() == 1;
}

void EdgeServerStateTracker::initializeCamerasTracking()
{
    const auto resourcePool = m_edgeServer->resourcePool();
    const auto allCameras =
        resourcePool->getAllCameras(QnResourcePtr(), /*ignoreDesktopCameras*/ true);

    for (const auto& camera: allCameras)
        trackCamera(camera);

    m_hasUniqueCoupledChildCameraPreviousValue = hasUniqueCoupledChildCamera();
    m_hasCanonicalStatePreviousValue = hasCanonicalState();

    m_camerasTrackingInitialized = true;
}

void EdgeServerStateTracker::trackCamera(const QnVirtualCameraResourcePtr& camera)
{
    using namespace nx::vms::common::utils;

    connect(camera.get(), &QnVirtualCameraResource::parentIdChanged,
        this, &EdgeServerStateTracker::onCameraParentIdChanged);

    const auto cameraId = camera->getId();

    if (camera->getParentId() == m_edgeServer->getId())
        m_childCamerasIds.insert(cameraId);

    if (edge_device::isCoupledEdgeCamera(toSharedPointer(m_edgeServer), camera))
    {
        m_coupledCamerasIds.insert(cameraId);
        connect(camera.get(), &QnVirtualCameraResource::nameChanged,
            this, &EdgeServerStateTracker::onCoupledCameraNameChanged);
    }
}

void EdgeServerStateTracker::updateState()
{
    if (m_hasUniqueCoupledChildCameraPreviousValue != hasUniqueCoupledChildCamera())
    {
        m_hasUniqueCoupledChildCameraPreviousValue = !m_hasUniqueCoupledChildCameraPreviousValue;
        emit hasCoupledCameraChanged();
    }

    if (m_hasCanonicalStatePreviousValue != hasCanonicalState())
    {
        m_hasCanonicalStatePreviousValue = !m_hasCanonicalStatePreviousValue;
        emit hasCanonicalStateChanged();
    }
}

void EdgeServerStateTracker::onResourceAdded(const QnResourcePtr& resource)
{
    if (!m_camerasTrackingInitialized)
    {
        if (resource == m_edgeServer)
            initializeCamerasTracking();
        return;
    }

    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        if (camera->hasFlags(Qn::desktop_camera))
            return;

        trackCamera(camera);

        updateState();
    }
}

void EdgeServerStateTracker::onResourceRemoved(const QnResourcePtr& resource)
{
    if (!m_camerasTrackingInitialized)
        return;

    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        if (camera->hasFlags(Qn::desktop_camera))
            return;

        camera->disconnect(this);

        const auto cameraId = camera->getId();

        m_coupledCamerasIds.remove(cameraId);
        m_childCamerasIds.remove(cameraId);

        updateState();
    }
}

void EdgeServerStateTracker::onCameraParentIdChanged(const QnResourcePtr& resource,
    const nx::Uuid& previousParentId)
{
    const auto camera = resource.staticCast<QnVirtualCameraResource>();
    if (camera->hasFlags(Qn::desktop_camera))
        return;

    if (previousParentId == m_edgeServer->getId())
        m_childCamerasIds.remove(camera->getId());

    if (camera->getParentId() == m_edgeServer->getId())
        m_childCamerasIds.insert(camera->getId());

    updateState();
}

void EdgeServerStateTracker::onCoupledCameraNameChanged(const QnResourcePtr& resource)
{
    if (hasUniqueCoupledChildCamera() && resource->getParentId() == m_edgeServer->getId())
        emit m_edgeServer->nameChanged(toSharedPointer(m_edgeServer));
}

} // namespace edge
} // namespace nx::core::resource
