#include "videowall_item_access_provider.h"

#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/helpers/layout_item_aggregator.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <common/common_module.h>

QnVideoWallItemAccessProvider::QnVideoWallItemAccessProvider(QObject* parent):
    base_type(parent),
    m_itemAggregator(new QnLayoutItemAggregator())
{
    connect(globalPermissionsManager(),
        &QnGlobalPermissionsManager::globalPermissionsChanged,
        this,
        &QnVideoWallItemAccessProvider::updateAccessBySubject);

    connect(m_itemAggregator, &QnLayoutItemAggregator::itemAdded, this,
        &QnVideoWallItemAccessProvider::handleItemAdded);
    connect(m_itemAggregator, &QnLayoutItemAggregator::itemRemoved, this,
        &QnVideoWallItemAccessProvider::handleItemRemoved);
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
    if (!globalPermissionsManager()->hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission))
        return false;

    if (resource->hasFlags(Qn::layout))
    {
        auto layout = resource.dynamicCast<QnLayoutResource>();
        NX_EXPECT(layout);
        return layout && m_itemAggregator->hasLayout(layout); /*< This method is called under mutex. */
    }

    if (!QnResourceAccessFilter::isShareableMedia(resource))
        return false;

    return m_itemAggregator->hasItem(resource->getId());
}

void QnVideoWallItemAccessProvider::fillProviders(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    QnResourceList& providers) const
{
    if (!globalPermissionsManager()->hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission))
        return;

    if (resource->hasFlags(Qn::layout))
    {
        auto videoWall = resource->getParentResource().dynamicCast<QnVideoWallResource>();
        if (videoWall)
            providers << videoWall;
        return;
    }

    if (!QnResourceAccessFilter::isShareableMedia(resource))
        return;

    /* Access to layout may be granted only by parent videowall resource. */
    auto resourceId = resource->getId();

    for (const auto& layout: m_itemAggregator->watchedLayouts())
    {
        for (const auto& item: layout->getItems())
        {
            if (item.resource.id == resourceId)
            {
                auto videoWall = layout->getParentResource().dynamicCast<QnVideoWallResource>();
                if (videoWall)
                    providers << videoWall << layout;
                break; /*< for item: getItems */
            }
        }
    }
}

void QnVideoWallItemAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    base_type::handleResourceAdded(resource);

    if (isUpdating())
        return;

    if (auto videoWall = resource.dynamicCast<QnVideoWallResource>())
    {
        handleVideoWallAdded(videoWall);
    }
    else if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout, &QnLayoutResource::parentIdChanged, this,
            [this, layout]
            {
                updateAccessToLayout(layout);
            });

        updateAccessToLayout(layout);
    }
}

QnLayoutResourceList QnVideoWallItemAccessProvider::getLayoutsForVideoWall(const QnVideoWallResourcePtr& videoWall) const
{
    return commonModule()->resourcePool()->getResourcesByParentId(videoWall->getId()).filtered<QnLayoutResource>();
}

void QnVideoWallItemAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    base_type::handleResourceRemoved(resource);

    /* Layouts and videowall can be removed independently. */

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        if (m_itemAggregator->removeWatchedLayout(layout))
            updateAccessToResource(layout);
    }
    else if (auto videoWall = resource.dynamicCast<QnVideoWallResource>())
    {
        for (auto layout: getLayoutsForVideoWall(videoWall))
        {
            if (m_itemAggregator->removeWatchedLayout(layout))
                updateAccessToResource(layout);
        }
    }
}

void QnVideoWallItemAccessProvider::afterUpdate()
{
    const auto& resPool = commonModule()->resourcePool();
    for (auto layout: resPool->getResources<QnLayoutResource>())
        updateAccessToLayout(layout);

    base_type::afterUpdate();
}

void QnVideoWallItemAccessProvider::handleVideoWallAdded(const QnVideoWallResourcePtr& videoWall)
{
    /* Layouts and videowalls can be added independently. */
    for (auto layout: getLayoutsForVideoWall(videoWall))
    {
        if (m_itemAggregator->addWatchedLayout(layout))
            updateAccessToResource(layout);
    }
}

void QnVideoWallItemAccessProvider::updateAccessToLayout(const QnLayoutResourcePtr& layout)
{
    /* Layouts and videowalls can be added independently. */
    auto parent = layout->getParentResource();
    if (parent && parent->hasFlags(Qn::videowall))
    {
        if (m_itemAggregator->addWatchedLayout(layout))
            updateAccessToResource(layout);
    }
}

void QnVideoWallItemAccessProvider::handleItemAdded(const QnUuid& resourceId)
{
    if (isUpdating())
        return;

    const auto& resPool = commonModule()->resourcePool();
    if (auto resource = resPool->getResourceById(resourceId))
        updateAccessToResource(resource);
}

void QnVideoWallItemAccessProvider::handleItemRemoved(const QnUuid& resourceId)
{
    if (isUpdating())
        return;

    const auto& resPool = commonModule()->resourcePool();
    if (auto resource = resPool->getResourceById(resourceId))
        updateAccessToResource(resource);
}
