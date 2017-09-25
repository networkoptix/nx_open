#include "videowall_item_access_provider.h"

#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/helpers/layout_item_aggregator.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <common/common_module.h>

using namespace nx::core::access;

namespace {

/**
 * We may have layout resources which have videowall as a parent but actually not located on the
 * videowall. Such are temporary layouts, that were saved to matrix. We must keep them alive to be
 * able to load matrix at any time, but these layouts must not provide access to the cameras.
 */
bool layoutBelongsToVideoWall(const QnResourcePtr& layout)
{
    NX_EXPECT(layout && layout->hasFlags(Qn::layout) && layout.dynamicCast<QnLayoutResource>());
    if (!layout)
        return false;

    const auto parentResource = layout->getParentResource();
    if (!parentResource || !parentResource->hasFlags(Qn::videowall))
        return false;

    const auto videowall = parentResource.dynamicCast<QnVideoWallResource>();
    NX_EXPECT(videowall);
    if (!videowall)
        return false;

    const auto items = videowall->items()->getItems();
    return std::any_of(items.cbegin(), items.cend(),
        [id = layout->getId()](const QnVideoWallItem& item)
        {
            return item.layout == id;
        });
}

}


QnVideoWallItemAccessProvider::QnVideoWallItemAccessProvider(
    Mode mode,
    QObject* parent)
    :
    base_type(mode, parent)
{
    if (mode == Mode::cached)
    {
        m_itemAggregator.reset(new QnLayoutItemAggregator());

        connect(globalPermissionsManager(),
            &QnGlobalPermissionsManager::globalPermissionsChanged,
            this,
            &QnVideoWallItemAccessProvider::updateAccessBySubject);

        connect(m_itemAggregator, &QnLayoutItemAggregator::itemAdded, this,
            &QnVideoWallItemAccessProvider::handleItemAdded);
        connect(m_itemAggregator, &QnLayoutItemAggregator::itemRemoved, this,
            &QnVideoWallItemAccessProvider::handleItemRemoved);
    }
}

QnVideoWallItemAccessProvider::~QnVideoWallItemAccessProvider()
{
}

Source QnVideoWallItemAccessProvider::baseSource() const
{
    return Source::videowall;
}

bool QnVideoWallItemAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!globalPermissionsManager()->hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission))
        return false;

    if (mode() == Mode::direct)
    {
        if (resource->hasFlags(Qn::layout))
            return layoutBelongsToVideoWall(resource);

        if (!QnResourceAccessFilter::isShareableMedia(resource))
            return false;

        // TODO: #GDM here resource splitting may help a lot: take all videowalls before, then
        //      go over all layouts, checking only if parent id is in set
        const auto resourceId = resource->getId();
        for (const auto& resource: commonModule()->resourcePool()->getResourcesWithFlag(Qn::layout))
        {
            if (!layoutBelongsToVideoWall(resource))
                continue;

            const auto layout = resource.dynamicCast<QnLayoutResource>();
            NX_EXPECT(layout);
            if (!layout)
                continue;
            for (const auto& item: layout->getItems())
            {
                if (item.resource.id == resourceId)
                    return true;
            }
        }

        return false;
    }


    NX_EXPECT(mode() == Mode::cached);

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
        if (layoutBelongsToVideoWall(resource))
            providers << resource->getParentResource();
        return;
    }

    if (!QnResourceAccessFilter::isShareableMedia(resource))
        return;

    /* Access to layout may be granted only by parent videowall resource. */
    auto resourceId = resource->getId();

    if (mode() == Mode::direct)
    {
        for (const auto& resource: commonModule()->resourcePool()->getResourcesWithFlag(Qn::layout))
        {
            if (!layoutBelongsToVideoWall(resource))
                continue;

            const auto layout = resource.dynamicCast<QnLayoutResource>();
            NX_EXPECT(layout);
            if (!layout)
                continue;

            for (const auto& item: layout->getItems())
            {
                if (item.resource.id != resourceId)
                    continue;
                providers << resource->getParentResource() << layout;
                break; //< 'for item: getItems()', go to the next layout.
            }
        }

        return;
    }

    NX_EXPECT(mode() == Mode::cached);
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
    NX_EXPECT(mode() == Mode::cached);

    base_type::handleResourceAdded(resource);

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

        if (!isUpdating())
            updateAccessToLayout(layout);
    }
}

QnLayoutResourceList QnVideoWallItemAccessProvider::getLayoutsForVideoWall(const QnVideoWallResourcePtr& videoWall) const
{
    return commonModule()->resourcePool()->getResourcesByParentId(videoWall->getId()).filtered<QnLayoutResource>();
}

void QnVideoWallItemAccessProvider::handleVideowallItemAdded(
    const QnVideoWallResourcePtr& videowall,
    const QnVideoWallItem& item)
{
    if (item.layout.isNull())
        return;

    // Layouts can be added to the pool after the videowall.
    const auto layout = resourcePool()->getResourceById<QnLayoutResource>(item.layout);
    if (!layout || layout->getParentId() != videowall->getId())
        return;

    if (m_itemAggregator->addWatchedLayout(layout))
        updateAccessToResource(layout);
}

void QnVideoWallItemAccessProvider::handleVideowallItemChanged(
    const QnVideoWallResourcePtr& videowall,
    const QnVideoWallItem& item,
    const QnVideoWallItem& oldItem)
{
    if (item.layout == oldItem.layout)
        return;

    handleVideowallItemRemoved(videowall, oldItem);
    handleVideowallItemAdded(videowall, item);
}

void QnVideoWallItemAccessProvider::handleVideowallItemRemoved(
    const QnVideoWallResourcePtr& /*videowall*/,
    const QnVideoWallItem &item)
{
    if (item.layout.isNull())
        return;

    // Layouts can be deleted before they are removed from the videowall.
    const auto layout = resourcePool()->getResourceById<QnLayoutResource>(item.layout);
    if (!layout)
        return;

    if (m_itemAggregator->removeWatchedLayout(layout))
        updateAccessToResource(layout);
}

void QnVideoWallItemAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    NX_EXPECT(mode() == Mode::cached);

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
    if (mode() == Mode::direct)
        return;

    const auto& resPool = commonModule()->resourcePool();
    for (auto layout: resPool->getResources<QnLayoutResource>())
        updateAccessToLayout(layout);

    base_type::afterUpdate();
}

void QnVideoWallItemAccessProvider::handleVideoWallAdded(const QnVideoWallResourcePtr& videoWall)
{
    NX_EXPECT(mode() == Mode::cached);

    /* Layouts and videowalls can be added independently. */
    if (!isUpdating())
    {
        for (auto layout: getLayoutsForVideoWall(videoWall))
        {
            if (layoutBelongsToVideoWall(layout) && m_itemAggregator->addWatchedLayout(layout))
                updateAccessToResource(layout);
        }
    }

    connect(videoWall, &QnVideoWallResource::itemAdded, this,
        &QnVideoWallItemAccessProvider::handleVideowallItemAdded);
    connect(videoWall, &QnVideoWallResource::itemChanged, this,
        &QnVideoWallItemAccessProvider::handleVideowallItemChanged);
    connect(videoWall, &QnVideoWallResource::itemRemoved, this,
        &QnVideoWallItemAccessProvider::handleVideowallItemRemoved);
}

void QnVideoWallItemAccessProvider::updateAccessToLayout(const QnLayoutResourcePtr& layout)
{
    NX_EXPECT(mode() == Mode::cached);

    // Layouts and videowalls can be added independently.
    if (layoutBelongsToVideoWall(layout) && m_itemAggregator->addWatchedLayout(layout))
        updateAccessToResource(layout);
}

void QnVideoWallItemAccessProvider::handleItemAdded(const QnUuid& resourceId)
{
    NX_EXPECT(mode() == Mode::cached);

    if (isUpdating())
        return;

    const auto& resPool = commonModule()->resourcePool();
    if (auto resource = resPool->getResourceById(resourceId))
        updateAccessToResource(resource);
}

void QnVideoWallItemAccessProvider::handleItemRemoved(const QnUuid& resourceId)
{
    NX_EXPECT(mode() == Mode::cached);

    if (isUpdating())
        return;

    const auto& resPool = commonModule()->resourcePool();
    if (auto resource = resPool->getResourceById(resourceId))
        updateAccessToResource(resource);
}
