// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_item_access_provider.h"

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/helpers/layout_item_aggregator.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>

namespace {

/**
 * We may have layout resources which have videowall as a parent but actually not located on the
 * videowall. Such are temporary layouts, that were saved to matrix. We must keep them alive to be
 * able to load matrix at any time, but these layouts must not provide access to the cameras.
 */
bool layoutBelongsToVideoWall(const QnResourcePtr& layout)
{
    NX_ASSERT(layout && layout->hasFlags(Qn::layout) && layout.dynamicCast<QnLayoutResource>());
    if (!layout)
        return false;

    const auto parentResource = layout->getParentResource();
    if (!parentResource || !parentResource->hasFlags(Qn::videowall))
        return false;

    const auto videowall = parentResource.dynamicCast<QnVideoWallResource>();
    NX_ASSERT(videowall);
    if (!videowall)
        return false;

    const auto items = videowall->items()->getItems();
    return std::any_of(items.cbegin(), items.cend(),
        [id = layout->getId()](const QnVideoWallItem& item)
        {
            return item.layout == id;
        });
}

} // namespace

namespace nx::core::access {

VideoWallItemAccessProvider::VideoWallItemAccessProvider(
    Mode mode,
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(mode, context, parent)
{
    if (mode == Mode::cached)
    {
        m_itemAggregator.reset(new QnLayoutItemAggregator());

        connect(m_context->globalPermissionsManager(),
            &QnGlobalPermissionsManager::globalPermissionsChanged,
            this,
            &VideoWallItemAccessProvider::updateAccessBySubject);

        connect(m_itemAggregator.get(), &QnLayoutItemAggregator::itemAdded,
            this, &VideoWallItemAccessProvider::handleItemAdded);

        connect(m_itemAggregator.get(), &QnLayoutItemAggregator::itemRemoved,
            this, &VideoWallItemAccessProvider::handleItemRemoved);
    }
}

VideoWallItemAccessProvider::~VideoWallItemAccessProvider()
{
}

Source VideoWallItemAccessProvider::baseSource() const
{
    return Source::videowall;
}

bool VideoWallItemAccessProvider::calculateAccess(const QnResourceAccessSubject& /*subject*/,
    const QnResourcePtr& resource,
    GlobalPermissions globalPermissions) const
{
    if (!globalPermissions.testFlag(GlobalPermission::controlVideowall))
        return false;

    if (mode() == Mode::direct)
    {
        if (resource->hasFlags(Qn::layout))
            return layoutBelongsToVideoWall(resource);

        if (!QnResourceAccessFilter::isShareableViaVideowall(resource))
            return false;

        // TODO: #sivanov Resource splitting may help a lot: take all videowalls before, then
        // go over all layouts, checking only if parent id is in set.
        const auto resourcePool = m_context->resourcePool();
        const auto resourceId = resource->getId();
        const auto layoutResources = resourcePool->getResourcesWithFlag(Qn::layout);

        for (const auto& resource: layoutResources)
        {
            if (!layoutBelongsToVideoWall(resource))
                continue;

            const auto layout = resource.dynamicCast<QnLayoutResource>();
            NX_ASSERT(layout);
            if (!layout)
                continue;

            const auto layoutItems = layout->getItems();
            for (const auto& item: layoutItems)
            {
                if (item.resource.id == resourceId)
                    return true;
            }
        }

        return false;
    }

    NX_ASSERT(mode() == Mode::cached);

    if (resource->hasFlags(Qn::layout))
    {
        auto layout = resource.dynamicCast<QnLayoutResource>();
        NX_ASSERT(layout);
        return layout && m_itemAggregator->hasLayout(layout); //< This method is called under mutex.
    }

    if (!QnResourceAccessFilter::isShareableViaVideowall(resource))
        return false;

    return m_itemAggregator->hasItem(resource->getId());
}

void VideoWallItemAccessProvider::fillProviders(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    QnResourceList& providers) const
{
    if (!m_context->globalPermissionsManager()->hasGlobalPermission(
            subject, GlobalPermission::controlVideowall))
    {
        return;
    }

    if (resource->hasFlags(Qn::layout))
    {
        if (layoutBelongsToVideoWall(resource))
            providers.append(resource->getParentResource());
        return;
    }

    if (!QnResourceAccessFilter::isShareableMedia(resource))
        return;

    // Access to layout may be granted only by parent videowall resource.
    const auto resourceId = resource->getId();

    if (mode() == Mode::direct)
    {
        const auto resourcePool = m_context->resourcePool();
        const auto layoutResources = resourcePool->getResourcesWithFlag(Qn::layout);

        for (const auto& resource: layoutResources)
        {
            if (!layoutBelongsToVideoWall(resource))
                continue;

            const auto layout = resource.dynamicCast<QnLayoutResource>();
            NX_ASSERT(layout);
            if (!layout)
                continue;

            const auto layoutItems = layout->getItems();
            for (const auto& item: layoutItems)
            {
                if (item.resource.id != resourceId)
                    continue;
                providers.append(resource->getParentResource());
                providers.append(layout);
                break; //< 'for item: getItems()', go to the next layout.
            }
        }

        return;
    }

    NX_ASSERT(mode() == Mode::cached);
    const auto watchedLayouts = m_itemAggregator->watchedLayouts();
    for (const auto& layout: watchedLayouts)
    {
        const auto layoutItems = layout->getItems();
        for (const auto& item: layoutItems)
        {
            if (item.resource.id == resourceId)
            {
                auto videoWall = layout->getParentResource().dynamicCast<QnVideoWallResource>();
                if (videoWall)
                {
                    providers.append(videoWall);
                    providers.append(layout);
                }
                break; //< for item: getItems
            }
        }
    }
}

void VideoWallItemAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    base_type::handleResourceAdded(resource);

    if (auto videoWall = resource.dynamicCast<QnVideoWallResource>())
    {
        handleVideoWallAdded(videoWall);
    }
    else if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout.get(), &QnLayoutResource::parentIdChanged, this,
            [this, layout]
            {
                updateAccessToLayout(layout);
            });

        if (!isUpdating())
            updateAccessToLayout(layout);
    }
}

QnLayoutResourceList VideoWallItemAccessProvider::getLayoutsForVideoWall(
    const QnVideoWallResourcePtr& videoWall) const
{
    return m_context->resourcePool()->getResourcesByParentId(videoWall->getId())
        .filtered<QnLayoutResource>();
}

void VideoWallItemAccessProvider::handleVideowallItemAdded(
    const QnVideoWallResourcePtr& videowall,
    const QnVideoWallItem& item)
{
    if (item.layout.isNull())
        return;

    // Layouts can be added to the pool after the videowall.
    const auto layout = m_context->resourcePool()->getResourceById<QnLayoutResource>(item.layout);
    if (!layout || layout->getParentId() != videowall->getId())
        return;

    if (m_itemAggregator->addWatchedLayout(layout))
        updateAccessToResource(layout);
}

void VideoWallItemAccessProvider::handleVideowallItemChanged(
    const QnVideoWallResourcePtr& videowall,
    const QnVideoWallItem& item,
    const QnVideoWallItem& oldItem)
{
    if (item.layout == oldItem.layout)
        return;

    handleVideowallItemRemoved(videowall, oldItem);
    handleVideowallItemAdded(videowall, item);
}

void VideoWallItemAccessProvider::handleVideowallItemRemoved(
    const QnVideoWallResourcePtr& /*videowall*/,
    const QnVideoWallItem &item)
{
    if (item.layout.isNull())
        return;

    // Layouts can be deleted before they are removed from the videowall.
    const auto layout = m_context->resourcePool()->getResourceById<QnLayoutResource>(item.layout);
    if (!layout)
        return;

    if (m_itemAggregator->removeWatchedLayout(layout))
        updateAccessToResource(layout);
}

void VideoWallItemAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    base_type::handleResourceRemoved(resource);

    // Layouts and videowall can be removed independently.
    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        if (m_itemAggregator->removeWatchedLayout(layout))
            updateAccessToResource(layout);
    }
    else if (auto videoWall = resource.dynamicCast<QnVideoWallResource>())
    {
        const auto layoutsForVideoWall = getLayoutsForVideoWall(videoWall);
        for (const auto& layout: layoutsForVideoWall)
        {
            if (m_itemAggregator->removeWatchedLayout(layout))
                updateAccessToResource(layout);
        }
    }
}

void VideoWallItemAccessProvider::afterUpdate()
{
    if (mode() == Mode::direct)
        return;

    const auto resourcePool = m_context->resourcePool();
    const auto layouts = resourcePool->getResources<QnLayoutResource>();

    for (const auto& layout: layouts)
        updateAccessToLayout(layout);

    base_type::afterUpdate();
}

void VideoWallItemAccessProvider::handleVideoWallAdded(const QnVideoWallResourcePtr& videoWall)
{
    NX_ASSERT(mode() == Mode::cached);

    // Layouts and videowalls can be added independently.
    if (!isUpdating())
    {
        const auto layoutsForVideoWall = getLayoutsForVideoWall(videoWall);
        for (const auto& layout: layoutsForVideoWall)
        {
            if (layoutBelongsToVideoWall(layout) && m_itemAggregator->addWatchedLayout(layout))
                updateAccessToResource(layout);
        }
    }

    connect(videoWall.get(), &QnVideoWallResource::itemAdded, this,
        &VideoWallItemAccessProvider::handleVideowallItemAdded);
    connect(videoWall.get(), &QnVideoWallResource::itemChanged, this,
        &VideoWallItemAccessProvider::handleVideowallItemChanged);
    connect(videoWall.get(), &QnVideoWallResource::itemRemoved, this,
        &VideoWallItemAccessProvider::handleVideowallItemRemoved);
}

void VideoWallItemAccessProvider::updateAccessToLayout(const QnLayoutResourcePtr& layout)
{
    NX_ASSERT(mode() == Mode::cached);

    // Layouts and videowalls can be added independently.
    if (layoutBelongsToVideoWall(layout) && m_itemAggregator->addWatchedLayout(layout))
        updateAccessToResource(layout);
}

void VideoWallItemAccessProvider::handleItemAdded(const QnUuid& resourceId)
{
    NX_ASSERT(mode() == Mode::cached);

    if (isUpdating())
        return;

    const auto resourcePool = m_context->resourcePool();
    if (auto resource = resourcePool->getResourceById(resourceId))
        updateAccessToResource(resource);
}

void VideoWallItemAccessProvider::handleItemRemoved(const QnUuid& resourceId)
{
    NX_ASSERT(mode() == Mode::cached);

    if (isUpdating())
        return;

    const auto resourcePool = m_context->resourcePool();
    if (auto resource = resourcePool->getResourceById(resourceId))
        updateAccessToResource(resource);
}

} // namespace nx::core::access
