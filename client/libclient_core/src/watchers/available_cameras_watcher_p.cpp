#include "available_cameras_watcher_p.h"

#include <core/resource_access/providers/resource_access_provider.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>

namespace detail {

Watcher::Watcher(const QnUserResourcePtr& user):
    m_user(user)
{
}

QnUserResourcePtr Watcher::user() const
{
    return m_user;
}

// LayoutBasedWatcher

LayoutBasedWatcher::LayoutBasedWatcher(const QnUserResourcePtr& user):
    Watcher(user)
{
    NX_ASSERT(user);
    if (!user)
        return;

    connect(qnResPool, &QnResourcePool::resourceAdded,
        this, &LayoutBasedWatcher::at_resourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved,
        this, &LayoutBasedWatcher::at_resourceRemoved);

    const auto layouts = qnResPool->getResourcesByParentId(
        user->getId()).filtered<QnLayoutResource>();

    for (const auto& layout: layouts)
        addLayout(layout);
}

QHash<QnUuid, QnVirtualCameraResourcePtr> LayoutBasedWatcher::cameras() const
{
    return m_cameras;
}

void LayoutBasedWatcher::at_resourceAdded(const QnResourcePtr& resource)
{
    const auto layout = resource.dynamicCast<QnLayoutResource>();
    if (!layout)
        return;

    connect(layout, &QnLayoutResource::parentIdChanged, this,
        [this](const QnResourcePtr& resource)
        {
            const auto layout = resource.dynamicCast<QnLayoutResource>();
            if (!layout)
                return;

            if (layout->getParentId() == user()->getId())
                addLayout(layout);
            else
                removeLayout(layout);
        });

    if (layout->getParentId() != user()->getId())
        return;

    addLayout(layout);
}

void LayoutBasedWatcher::at_resourceRemoved(const QnResourcePtr& resource)
{
    if (const auto layout = resource.dynamicCast<QnLayoutResource>())
        removeLayout(layout);
}

void LayoutBasedWatcher::addLayout(const QnLayoutResourcePtr& layout)
{
    connect(layout, &QnLayoutResource::itemAdded,
        this, &LayoutBasedWatcher::at_layoutItemAdded);
    connect(layout, &QnLayoutResource::itemRemoved,
        this, &LayoutBasedWatcher::at_layoutItemRemoved);

    for (const auto& item: layout->getItems())
        at_layoutItemAdded(layout, item);
}

void LayoutBasedWatcher::removeLayout(const QnLayoutResourcePtr& layout)
{
    layout->disconnect(this);

    for (const auto& item: layout->getItems())
        at_layoutItemRemoved(layout, item);
}

void LayoutBasedWatcher::at_layoutItemAdded(
    const QnLayoutResourcePtr& /*resource*/, const QnLayoutItemData& item)
{
    const auto id = item.resource.id;
    if (id.isNull())
        return;

    const auto camera = qnResPool->getResourceById<QnVirtualCameraResource>(id);
    if (!camera)
        return;

    m_cameras[id] = camera;
    if (m_camerasCounter.insert(id))
        emit cameraAdded(camera);
}

void LayoutBasedWatcher::at_layoutItemRemoved(
    const QnLayoutResourcePtr& /*resource*/, const QnLayoutItemData& item)
{
    const auto id = item.resource.id;
    if (id.isNull())
        return;

    const auto camera = qnResPool->getResourceById<QnVirtualCameraResource>(id);

    m_cameras.remove(id);
    if (m_camerasCounter.remove(id))
        emit cameraRemoved(camera);
}

// PermissionsBasedWatcher

PermissionsBasedWatcher::PermissionsBasedWatcher(const QnUserResourcePtr& user):
    Watcher(user)
{
    if (user)
    {
        connect(qnResourceAccessProvider, &QnResourceAccessProvider::accessChanged, this,
            [this, user](const QnResourceAccessSubject& subject, const QnResourcePtr& resource)
            {
                if (!subject.isUser() || subject.user() != user)
                    return;

                if (qnResourceAccessProvider->hasAccess(user, resource))
                    at_resourceAdded(resource);
                else
                    at_resourceRemoved(resource);
            });
    }

    connect(qnResPool, &QnResourcePool::resourceAdded,
        this, &PermissionsBasedWatcher::at_resourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved,
        this, &PermissionsBasedWatcher::at_resourceRemoved);

    for (const auto& camera: qnResPool->getAllCameras(QnResourcePtr(), true))
        at_resourceAdded(camera);
}

QHash<QnUuid, QnVirtualCameraResourcePtr> PermissionsBasedWatcher::cameras() const
{
    return m_cameras;
}

void PermissionsBasedWatcher::at_resourceAdded(const QnResourcePtr& resource)
{
    const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    if (user() && !qnResourceAccessProvider->hasAccess(user(), resource))
        return;

    m_cameras[camera->getId()] = camera;
    emit cameraAdded(resource);
}

void PermissionsBasedWatcher::at_resourceRemoved(const QnResourcePtr& resource)
{
    const auto camera = m_cameras.take(resource->getId());
    if (!camera)
        return;

    emit cameraRemoved(resource);
}

} // namespace detail
