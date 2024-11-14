// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "license_usage_watcher.h"

#include <api/runtime_info_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <licensing/license.h>
#include <nx/vms/common/system_context.h>

namespace nx::vms::common {

LicenseUsageWatcher::LicenseUsageWatcher(SystemContext* context, QObject* parent):
    base_type(parent),
    SystemContextAware(context)
{
    connect(context->licensePool(), &QnLicensePool::licensesChanged, this,
        &LicenseUsageWatcher::licenseUsageChanged);

    // Call update if server was added or removed or changed its status.
    auto updateIfServerStatusChanged =
        [this](const QnPeerRuntimeInfo& data)
        {
            if (data.data.peer.peerType == nx::vms::api::PeerType::server)
                emit licenseUsageChanged();
        };

    // Ignoring runtimeInfoChanged as hardwareIds must not change in runtime.
    connect(context->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoAdded, this,
        updateIfServerStatusChanged);
    connect(context->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoRemoved, this,
        updateIfServerStatusChanged);
}

DeviceLicenseUsageWatcher::DeviceLicenseUsageWatcher(SystemContext* context, QObject* parent):
    base_type(context, parent)
{
    const auto& resPool = context->resourcePool();

    // Listening to all changes that can affect licenses usage.
    auto connectToCamera =
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            connect(camera.get(), &QnVirtualCameraResource::scheduleEnabledChanged, this,
                &LicenseUsageWatcher::licenseUsageChanged);
            connect(camera.get(), &QnVirtualCameraResource::groupNameChanged, this,
                &LicenseUsageWatcher::licenseUsageChanged);
            connect(camera.get(), &QnVirtualCameraResource::groupIdChanged, this,
                &LicenseUsageWatcher::licenseUsageChanged);
            connect(camera.get(), &QnVirtualCameraResource::licenseTypeChanged, this,
                &LicenseUsageWatcher::licenseUsageChanged);
            connect(camera.get(), &QnVirtualCameraResource::parentIdChanged, this,
                &LicenseUsageWatcher::licenseUsageChanged);
    };

    // Call update if camera was added or removed.
    auto updateIfNeeded =
        [this](const QnResourcePtr &resource)
        {
            if (resource.dynamicCast<QnVirtualCameraResource>())
                emit licenseUsageChanged();
        };

    connect(resPool, &QnResourcePool::resourceAdded, this, updateIfNeeded);
    connect(resPool, &QnResourcePool::resourceRemoved, this, updateIfNeeded);

    connect(resPool, &QnResourcePool::resourceAdded, this,
        [connectToCamera](const QnResourcePtr& resource)
        {
            if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
                connectToCamera(camera);
        });

    connect(resPool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
                camera->disconnect(this);
        });

    for (const auto& camera: resPool->getAllCameras(QnResourcePtr(), true))
        connectToCamera(camera);
}

VideoWallLicenseUsageWatcher::VideoWallLicenseUsageWatcher(
    SystemContext* context,
    QObject* parent)
    :
    base_type(context, parent)
{
    auto connectTo =
        [this](const QnVideoWallResourcePtr& videowall)
        {
            connect(videowall.get(),
                &QnVideoWallResource::itemAdded,
                this,
                &LicenseUsageWatcher::licenseUsageChanged);
            connect(videowall.get(),
                &QnVideoWallResource::itemRemoved,
                this,
                &LicenseUsageWatcher::licenseUsageChanged);
        };

    auto resourceAdded =
        [this, connectTo](const QnResourcePtr& resource)
        {
            if (const auto videowall = resource.dynamicCast<QnVideoWallResource>())
            {
                connectTo(videowall);
                emit licenseUsageChanged();
            }
        };

    auto resourceRemoved =
        [this](const QnResourcePtr& resource)
        {
            if (const auto videowall = resource.dynamicCast<QnVideoWallResource>())
            {
                videowall->disconnect(this);
                emit licenseUsageChanged();
            }
        };

    const auto& resPool = context->resourcePool();

    connect(resPool, &QnResourcePool::resourceAdded, this, resourceAdded);
    connect(resPool, &QnResourcePool::resourceRemoved, this, resourceRemoved);
    for (const auto& videowall: resPool->getResources<QnVideoWallResource>())
        connectTo(videowall);
}

} // namespace nx::vms::common
