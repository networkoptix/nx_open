// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "edge_server_state_tracker.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

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

void EdgeServerStateTracker::initializeCamerasTracking()
{
    const auto resourcePool = m_edgeServer->resourcePool();

    const auto allCameras =
        resourcePool->getAllCameras(QnResourcePtr(), /*ignoreDesktopCameras*/ true);

    for (const auto& camera: allCameras)
    {
        connect(camera.get(), &QnVirtualCameraResource::parentIdChanged,
            this, &EdgeServerStateTracker::onCameraParentIdChanged);

        if (camera->getParentId() == m_edgeServer->getId())
            m_childCamerasIds.insert(camera->getId());

        if (isCoupledCamera(camera))
            m_coupledCamera = camera;
    }

    if (hasCoupledCamera())
    {
        connect(m_coupledCamera.get(), &QnVirtualCameraResource::nameChanged, this,
            [this] { emit m_edgeServer->nameChanged(toSharedPointer(m_edgeServer)); });
    }

    m_hasCoupledCameraPreviousValue = hasCoupledCamera();
    m_hasCanonicalStatePreviousValue = hasCanonicalState();

    m_camerasTrackingInitialized = true;
}

bool EdgeServerStateTracker::hasCoupledCamera() const
{
    return !m_coupledCamera.isNull() && (m_coupledCamera->getParentId() == m_edgeServer->getId());
}

bool EdgeServerStateTracker::hasCanonicalState() const
{
    return hasCoupledCamera() && m_childCamerasIds.size() == 1;
}

QnVirtualCameraResourcePtr EdgeServerStateTracker::coupledCamera() const
{
    return m_coupledCamera;
}

void EdgeServerStateTracker::onResourceAdded(const QnResourcePtr& resource)
{
    if (!m_camerasTrackingInitialized)
    {
        if (resource != m_edgeServer)
            return;

        initializeCamerasTracking();
    }

    if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        if (camera->hasFlags(Qn::desktop_camera))
            return;

        connect(camera.get(), &QnVirtualCameraResource::parentIdChanged,
            this, &EdgeServerStateTracker::onCameraParentIdChanged);

        if (camera->getParentId() == m_edgeServer->getId())
            m_childCamerasIds.insert(camera->getId());

        if (isCoupledCamera(camera))
            m_coupledCamera = camera;

        updateHasCoupledCamera();
        updateHasCanonicalState();
    }
}

void EdgeServerStateTracker::onResourceRemoved(const QnResourcePtr& resource)
{
    if (!m_camerasTrackingInitialized)
        return;

    if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        if (camera->hasFlags(Qn::desktop_camera))
            return;

        if (isCoupledCamera(camera))
        {
            m_coupledCamera->disconnect(this);
            m_coupledCamera = QnVirtualCameraResourcePtr();
        }

        if (camera->getParentId() == m_edgeServer->getId())
            m_childCamerasIds.remove(camera->getId());

        updateHasCoupledCamera();
        updateHasCanonicalState();
    }
}

void EdgeServerStateTracker::onCameraParentIdChanged(const QnResourcePtr& resource,
    const QnUuid& previousParentId)
{
    const auto camera = resource.staticCast<QnVirtualCameraResource>();
    if (camera->hasFlags(Qn::desktop_camera))
        return;

    if (previousParentId == m_edgeServer->getId())
        m_childCamerasIds.remove(camera->getId());

    if (camera->getParentId() == m_edgeServer->getId())
        m_childCamerasIds.insert(camera->getId());

    updateHasCoupledCamera();
    updateHasCanonicalState();
}

bool EdgeServerStateTracker::isCoupledCamera(const QnVirtualCameraResourcePtr& camera) const
{
    using namespace nx::network;

    if (camera->hasFlags(Qn::virtual_camera) || camera->hasFlags(Qn::desktop_camera))
        return false;

    const auto cameraHostAddressString = camera->getHostAddress();
    if (cameraHostAddressString.isEmpty())
        return false;

    const auto cameraHostAddress = HostAddress(cameraHostAddressString);
    const auto serverAddressList = m_edgeServer->getNetAddrList();

    return std::any_of(std::cbegin(serverAddressList), std::cend(serverAddressList),
        [&cameraHostAddress](const auto& serverAddress)
        {
            return serverAddress.address == cameraHostAddress;
        });
}

void EdgeServerStateTracker::updateHasCoupledCamera()
{
    if (m_hasCoupledCameraPreviousValue == hasCoupledCamera())
        return;

    if (hasCoupledCamera())
    {
        connect(coupledCamera().get(), &QnVirtualCameraResource::nameChanged, this,
            [this] { emit m_edgeServer->nameChanged(toSharedPointer(m_edgeServer)); });
    }
    else if (coupledCamera())
    {
        coupledCamera()->disconnect(this);
    }

    m_hasCoupledCameraPreviousValue = !m_hasCoupledCameraPreviousValue;
    emit hasCoupledCameraChanged();
}

void EdgeServerStateTracker::updateHasCanonicalState()
{
    if (m_hasCanonicalStatePreviousValue == hasCanonicalState())
        return;

    m_hasCanonicalStatePreviousValue = !m_hasCanonicalStatePreviousValue;
    emit hasCanonicalStateChanged();
}

} // namespace edge
} // namespace nx::core::resource
