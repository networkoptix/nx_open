#include "videowall_item_access_provider.h"

#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_filter.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

QnVideoWallItemAccessProvider::QnVideoWallItemAccessProvider(QObject* parent):
    base_type(parent)
{
    connect(qnGlobalPermissionsManager, &QnGlobalPermissionsManager::globalPermissionsChanged,
        this, &QnVideoWallItemAccessProvider::updateAccessBySubject);
}

QnVideoWallItemAccessProvider::~QnVideoWallItemAccessProvider()
{
}

QnAbstractResourceAccessProvider::Source QnVideoWallItemAccessProvider::baseSource() const
{
    return Source::videowall;
}

bool QnVideoWallItemAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!qnGlobalPermissionsManager->hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission))
        return false;

    if (resource->hasFlags(Qn::layout))
    {
        auto layout = resource.dynamicCast<QnLayoutResource>();
        NX_ASSERT(layout);
        return layout && m_accessibleLayouts.contains(layout); /*< This method is called under mutex. */
    }

    if (!QnResourceAccessFilter::isShareableMedia(resource))
        return false;

    auto resourceId = resource->getId();
    for (const auto& layout : m_accessibleLayouts)
    {
        for (const auto& item : layout->getItems())
        {
            if (item.resource.id == resourceId)
                return true;
        }
    }

    return false;
}

void QnVideoWallItemAccessProvider::fillProviders(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    QnResourceList& providers) const
{
    if (!qnGlobalPermissionsManager->hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission))
        return;

    /* Access to layout may be granted only by parent videowall resource. */
    auto layouts = accessibleLayouts();

    if (resource->hasFlags(Qn::layout))
    {
        auto videoWall = resource->getParentResource().dynamicCast<QnVideoWallResource>();
        if (videoWall)
            providers << videoWall;
        return;
    }

    if (!QnResourceAccessFilter::isShareableMedia(resource))
        return;

    auto resourceId = resource->getId();
    for (const auto& layout : layouts)
    {
        for (const auto& item : layout->getItems())
        {
            if (item.resource.id == resourceId)
            {
                auto videoWall = layout->getParentResource().dynamicCast<QnVideoWallResource>();
                if (videoWall)
                    providers << videoWall;
                break; /*< for item: getItems */
            }
        }
    }
}

void QnVideoWallItemAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    base_type::handleResourceAdded(resource);

    if (auto videoWall = resource.dynamicCast<QnVideoWallResource>())
    {
        handleVideoWallAdded(videoWall);
    }
    else if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        handleLayoutAdded(layout);
    }
}

void QnVideoWallItemAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    base_type::handleResourceRemoved(resource);

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        if (isUpdating())
            return;

        {
            QnMutexLocker lk(&m_mutex);
            m_accessibleLayouts.remove(layout);
        }

        for (const auto& resource : layout->layoutResources())
            updateAccessToResource(resource);
    }
    else if (auto videoWall = resource.dynamicCast<QnVideoWallResource>())
    {
        if (isUpdating())
            return;

        QnUuid videoWallId = videoWall->getId();
        QSet<QnLayoutResourcePtr> removedLayouts;
        {
            QnMutexLocker lk(&m_mutex);
            for (auto layout : m_accessibleLayouts)
            {
                if (layout->getParentId() == videoWallId)
                    removedLayouts.insert(layout);
            }
            m_accessibleLayouts -= removedLayouts;
        }

        for (auto layout: removedLayouts)
            updateByLayout(layout);
    }
}

void QnVideoWallItemAccessProvider::afterUpdate()
{
    updateAccessibleLayouts();
    base_type::afterUpdate();
}

void QnVideoWallItemAccessProvider::handleVideoWallAdded(const QnVideoWallResourcePtr& videoWall)
{
    if (!isUpdating())
    {
        const auto childLayouts = qnResPool->getResourcesWithParentId(videoWall->getId())
            .filtered<QnLayoutResource>();

        {
            QnMutexLocker lk(&m_mutex);
            for (const auto& child: childLayouts)
                m_accessibleLayouts.insert(child);
        }

        for (const auto& child: childLayouts)
            updateByLayout(child);
    }
}

void QnVideoWallItemAccessProvider::handleLayoutAdded(const QnLayoutResourcePtr& layout)
{
    auto handleItemChanged =
        [this](const QnLayoutResourcePtr& layout, const QnLayoutItemData& item)
        {
            if (isUpdating())
                return;

            /* Check only layouts that belong to videowall. */
            if (!accessibleLayouts().contains(layout))
                return;

            /* Only remote resources with correct id can be accessed. */
            if (item.resource.id.isNull())
                return;

            if (auto resource = qnResPool->getResourceById(item.resource.id))
                updateAccessToResource(resource);
        };

    /* Connect to all layouts. Possibly videowall will be added to the pool later. */
    connect(layout, &QnLayoutResource::itemAdded, this, handleItemChanged);
    connect(layout, &QnLayoutResource::itemRemoved, this, handleItemChanged);

    if (!isUpdating())
    {
        auto parent = layout->getParentResource();
        if (parent && parent->hasFlags(Qn::videowall))
        {
            {
                QnMutexLocker lk(&m_mutex);
                m_accessibleLayouts.insert(layout);
            }
            for (const auto& resource: layout->layoutResources())
                updateAccessToResource(resource);
        }
    }
}

void QnVideoWallItemAccessProvider::updateByLayout(const QnLayoutResourcePtr& layout)
{
    if (isUpdating())
        return;

    NX_ASSERT(layout);
    if (!layout)
        return;

    updateAccessToResource(layout);
    for (const auto& resource: layout->layoutResources())
        updateAccessToResource(resource);
}

void QnVideoWallItemAccessProvider::updateAccessibleLayouts()
{
    if (isUpdating())
        return;

    QSet<QnLayoutResourcePtr> result;
    for (const auto& layout : qnResPool->getResources<QnLayoutResource>())
    {
        auto parent = layout->getParentResource();
        if (parent && parent->hasFlags(Qn::videowall))
            result << layout;
    }

    QnMutexLocker lk(&m_mutex);
    m_accessibleLayouts = result;
}

QSet<QnLayoutResourcePtr> QnVideoWallItemAccessProvider::accessibleLayouts() const
{
    QnMutexLocker lk(&m_mutex);
    return m_accessibleLayouts;
}
