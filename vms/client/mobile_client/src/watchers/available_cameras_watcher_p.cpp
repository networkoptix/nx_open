// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "available_cameras_watcher_p.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>

using namespace nx::core::access;

namespace detail {

Watcher::Watcher(const QnUserResourcePtr& user, nx::vms::common::SystemContext* systemContext):
    SystemContextAware(systemContext),
    m_user(user)
{
}

QnUserResourcePtr Watcher::user() const
{
    return m_user;
}

// LayoutBasedWatcher

LayoutBasedWatcher::LayoutBasedWatcher(const QnUserResourcePtr& user,
    nx::vms::common::SystemContext* systemContext)
    :
    Watcher(user, systemContext),
    m_itemAggregator(new QnLayoutItemAggregator(this))
{
    NX_ASSERT(user);
    if (!user)
        return;

    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, &LayoutBasedWatcher::at_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, &LayoutBasedWatcher::at_resourceRemoved);

    connect(m_itemAggregator, &QnLayoutItemAggregator::itemAdded,
        this, &LayoutBasedWatcher::at_layoutItemAdded);
    connect(m_itemAggregator, &QnLayoutItemAggregator::itemRemoved,
        this, &LayoutBasedWatcher::at_layoutItemRemoved);

    const auto layouts = resourcePool()->getResourcesByParentId(
        user->getId()).filtered<QnLayoutResource>();

    for (const auto& layout: layouts)
        m_itemAggregator->addWatchedLayout(layout);
}

QHash<nx::Uuid, QnVirtualCameraResourcePtr> LayoutBasedWatcher::cameras() const
{
    return m_cameras;
}

void LayoutBasedWatcher::at_resourceAdded(const QnResourcePtr& resource)
{
    const auto layout = resource.dynamicCast<QnLayoutResource>();
    if (!layout)
        return;

    connect(layout.get(), &QnLayoutResource::parentIdChanged, this,
        [this](const QnResourcePtr& resource)
        {
            const auto layout = resource.dynamicCast<QnLayoutResource>();
            if (!layout)
                return;

            if (layout->getParentId() == user()->getId())
                m_itemAggregator->addWatchedLayout(layout);
            else
                m_itemAggregator->removeWatchedLayout(layout);
        });

    if (layout->getParentId() != user()->getId())
        return;

    m_itemAggregator->addWatchedLayout(layout);
}

void LayoutBasedWatcher::at_resourceRemoved(const QnResourcePtr& resource)
{
    if (const auto layout = resource.dynamicCast<QnLayoutResource>())
        m_itemAggregator->removeWatchedLayout(layout);
}

void LayoutBasedWatcher::at_layoutItemAdded(const nx::Uuid& id)
{
    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);
    if (!camera)
        return;

    m_cameras[id] = camera;
    emit cameraAdded(camera);
}

void LayoutBasedWatcher::at_layoutItemRemoved(const nx::Uuid& id)
{
    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);

    m_cameras.remove(id);
    emit cameraRemoved(camera);
}

// PermissionsBasedWatcher

PermissionsBasedWatcher::PermissionsBasedWatcher(const QnUserResourcePtr& user,
    nx::vms::common::SystemContext* systemContext)
    :
    Watcher(user, systemContext)
{
    if (!NX_ASSERT(user))
        return;

    const auto processCameras =
        [this, user](const QnVirtualCameraResourceList& cameras)
        {
            for (const auto& camera: cameras)
            {
                if (resourceAccessManager()->hasPermission(user, camera, Qn::ViewContentPermission))
                    addCamera(camera);
                else
                    removeCamera(camera);
            }
        };

    const auto updateAccess =
        [this, processCameras]()
        {
            const auto cameras = resourcePool()->getAllCameras(/*server*/ QnResourcePtr(), true);
            processCameras(cameras);
        };

    const auto notifier = resourceAccessManager()->createNotifier(this);
    notifier->setSubjectId(user->getId());

    connect(notifier, &QnResourceAccessManager::Notifier::resourceAccessChanged,
        this, updateAccess);

    connect(resourceAccessManager(), &QnResourceAccessManager::resourceAccessReset,
        this, updateAccess);

    const auto updateCameras =
        [processCameras](const QnResourceList& resources)
        {
            const auto cameras = resources.filtered<QnVirtualCameraResource>();
            processCameras(cameras);
        };

    connect(resourcePool(), &QnResourcePool::resourcesAdded, this, updateCameras);
    connect(resourcePool(), &QnResourcePool::resourcesRemoved, this, updateCameras);

    updateAccess();
}

QHash<nx::Uuid, QnVirtualCameraResourcePtr> PermissionsBasedWatcher::cameras() const
{
    return m_cameras;
}

void PermissionsBasedWatcher::addCamera(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    if (camera->hasFlags(Qn::desktop_camera))
        return;

    const auto id = camera->getId();
    if (m_cameras.contains(id))
        return;

    m_cameras.insert(id, camera);
    emit cameraAdded(resource);
}

void PermissionsBasedWatcher::removeCamera(const QnResourcePtr& resource)
{
    const auto camera = m_cameras.take(resource->getId());
    if (!camera)
        return;

    emit cameraRemoved(resource);
}

} // namespace detail
