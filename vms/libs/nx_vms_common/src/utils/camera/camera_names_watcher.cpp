// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_names_watcher.h"

#include <core/resource/resource_display_info.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>

using namespace nx::vms::common;

QnCameraNamesWatcher::QnCameraNamesWatcher(SystemContext* systemContext):
    base_type(nullptr),
    SystemContextAware(systemContext),
    m_names()
{
}

QnCameraNamesWatcher::~QnCameraNamesWatcher()
{
}

QString QnCameraNamesWatcher::getCameraName(const nx::Uuid& cameraId)
{
    const auto it = m_names.find(cameraId);
    if (it != m_names.end())
        return *it;

    const auto& resPool = resourcePool();

    const auto cameraResource = resPool->getResourceById<QnVirtualCameraResource>(cameraId);
    if (!cameraResource)
        return nx::format("<%1>", tr("Removed camera"));

    connect(resPool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            const auto cameraResource = resource.dynamicCast<QnVirtualCameraResource>();
            if (!cameraResource)
                return;

            m_names.remove(cameraResource->getId());
            disconnect(cameraResource.data(), nullptr, this, nullptr);
        });

    connect(cameraResource.data(), &QnVirtualCameraResource::nameChanged, this,
        [this, cameraId](const QnResourcePtr &resource)
        {
            const auto newName = QnResourceDisplayInfo(resource).toString(Qn::RI_NameOnly);
            m_names[cameraId] = newName;
            emit cameraNameChanged(cameraId);
        });

    const auto cameraName = QnResourceDisplayInfo(cameraResource).toString(Qn::RI_NameOnly);
    m_names[cameraId] = cameraName;
    return cameraName;
}
