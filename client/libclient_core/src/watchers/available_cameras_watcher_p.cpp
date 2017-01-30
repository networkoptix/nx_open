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
    Watcher(user),
    m_itemAggregator(new QnLayoutItemAggregator(this))
{
    NX_ASSERT(user);
    if (!user)
        return;

    connect(qnResPool, &QnResourcePool::resourceAdded,
        this, &LayoutBasedWatcher::at_resourceAdded);
    connect(qnResPool, &QnResourcePool::resourceRemoved,
        this, &LayoutBasedWatcher::at_resourceRemoved);

    connect(m_itemAggregator, &QnLayoutItemAggregator::itemAdded,
        this, &LayoutBasedWatcher::at_layoutItemAdded);
    connect(m_itemAggregator, &QnLayoutItemAggregator::itemRemoved,
        this, &LayoutBasedWatcher::at_layoutItemRemoved);

    const auto layouts = qnResPool->getResourcesByParentId(
        user->getId()).filtered<QnLayoutResource>();

    for (const auto& layout: layouts)
        m_itemAggregator->addWatchedLayout(layout);
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

void LayoutBasedWatcher::at_layoutItemAdded(const QnUuid& id)
{
    const auto camera = qnResPool->getResourceById<QnVirtualCameraResource>(id);
    if (!camera)
        return;

    m_cameras[id] = camera;
    emit cameraAdded(camera);
}

void LayoutBasedWatcher::at_layoutItemRemoved(const QnUuid& id)
{
    const auto camera = qnResPool->getResourceById<QnVirtualCameraResource>(id);

    m_cameras.remove(id);
    emit cameraRemoved(camera);
}

// PermissionsBasedWatcher

PermissionsBasedWatcher::PermissionsBasedWatcher(const QnUserResourcePtr& user):
    Watcher(user)
{
    const auto accessProvider = qnResourceAccessProvider;

    if (user)
    {
        connect(accessProvider, &QnResourceAccessProvider::accessChanged, this,
            [this, accessProvider, user]
            (const QnResourceAccessSubject& subject, const QnResourcePtr& resource)
            {
                if (!subject.isUser() || subject.user() != user)
                    return;

                if (accessProvider->hasAccess(user, resource))
                    addCamera(resource);
                else
                    removeCamera(resource);
            });
    }

    for (const auto& camera: qnResPool->getAllCameras(QnResourcePtr(), true))
    {
        if (accessProvider->hasAccess(user, camera))
            addCamera(camera);
    }
}

QHash<QnUuid, QnVirtualCameraResourcePtr> PermissionsBasedWatcher::cameras() const
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

    const auto key = camera->getId();
    const auto existing = m_cameras.find(key);
    if (existing != m_cameras.cend())
    {
        NX_ASSERT(*existing == camera);
        return;
    }

    m_cameras.insert(key, camera);
    emit cameraAdded(resource);
}

void PermissionsBasedWatcher::removeCamera(const QnResourcePtr& resource)
{
    const auto camera = m_cameras.take(resource->getId());
    NX_ASSERT(camera);
    if (!camera)
        return;

    emit cameraRemoved(resource);
}

} // namespace detail
