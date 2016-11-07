#include "videowall_item_access_provider.h"

#include <core/resource_access/global_permissions_manager.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

namespace {

QSet<QnUuid> videoWallLayouts(const QnVideoWallResourcePtr& videoWall)
{
    QSet<QnUuid> result;
    for (const auto& item : videoWall->items()->getItems())
    {
        if (item.layout.isNull())
            continue;
        result << item.layout;
    }
    return result;
}

} // namespace

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

    QSet<QnUuid> layoutIds = accessibleLayouts();
    auto resourceId = resource->getId();
    if (layoutIds.contains(resourceId))
        return true;

    auto layouts = qnResPool->getResources<QnLayoutResource>(layoutIds);

    for (const auto& layout : layouts)
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

    auto resourceId = resource->getId();
    for (const auto& videoWall : qnResPool->getResources<QnVideoWallResource>())
    {
        QSet<QnUuid> layoutIds = videoWallLayouts(videoWall);
        if (layoutIds.contains(resourceId))
        {
            providers << videoWall;
            continue;
        }
        auto layouts = qnResPool->getResources<QnLayoutResource>(layoutIds);
        for (const auto& layout : layouts)
        {
            for (const auto& item : layout->getItems())
            {
                if (item.resource.id == resourceId)
                {
                    providers << videoWall;
                    break;
                }
            }
        }
    }
}

void QnVideoWallItemAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    base_type::handleResourceAdded(resource);

    if (auto videoWall = resource.dynamicCast<QnVideoWallResource>())
    {
        connect(videoWall, &QnVideoWallResource::itemAdded, this,
            &QnVideoWallItemAccessProvider::handleVideoWallItemAdded);
        connect(videoWall, &QnVideoWallResource::itemChanged, this,
            &QnVideoWallItemAccessProvider::handleVideoWallItemChanged);
        connect(videoWall, &QnVideoWallResource::itemRemoved, this,
            &QnVideoWallItemAccessProvider::handleVideoWallItemRemoved);
        updateAccessToVideoWallItems(videoWall);
    }
    else if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        auto handleItemChanged =
            [this](const QnLayoutResourcePtr& layout, const QnLayoutItemData& item)
        {
            /* Check only layouts that belong to videowall. */
            if (!accessibleLayouts().contains(layout->getId()))
                return;

            /* Only remote resources with correct id can be accessed. */
            if (item.resource.id.isNull())
                return;

            if (auto resource = qnResPool->getResourceById(item.resource.id))
                updateAccessToResource(resource);
        };

        connect(layout, &QnLayoutResource::itemAdded, this, handleItemChanged);
        connect(layout, &QnLayoutResource::itemRemoved, this, handleItemChanged);
    }
}

void QnVideoWallItemAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    base_type::handleResourceRemoved(resource);
    if (auto videoWall = resource.dynamicCast<QnVideoWallResource>())
        updateAccessToVideoWallItems(videoWall);
}

void QnVideoWallItemAccessProvider::handleVideoWallItemAdded(
    const QnVideoWallResourcePtr& /*resource*/, const QnVideoWallItem &item)
{
    updateByLayoutId(item.layout);
}

void QnVideoWallItemAccessProvider::handleVideoWallItemChanged(
    const QnVideoWallResourcePtr& /*resource*/,
    const QnVideoWallItem& item,
    const QnVideoWallItem& oldItem)
{
    if (oldItem.layout == item.layout)
        return;

    updateByLayoutId(item.layout);
    updateByLayoutId(oldItem.layout);
}

void QnVideoWallItemAccessProvider::handleVideoWallItemRemoved(
    const QnVideoWallResourcePtr& /*resource*/, const QnVideoWallItem &item)
{
    updateByLayoutId(item.layout);
}

void QnVideoWallItemAccessProvider::updateByLayoutId(const QnUuid& id)
{
    if (id.isNull())
        return;

    if (auto layout = qnResPool->getResourceById<QnLayoutResource>(id))
    {
        updateAccessToResource(layout);

        for (const auto& resource: layout->layoutResources())
            updateAccessToResource(resource);
    }
}

void QnVideoWallItemAccessProvider::updateAccessToVideoWallItems(
    const QnVideoWallResourcePtr& videoWall)
{
    for (auto layoutId: videoWallLayouts(videoWall))
        updateByLayoutId(layoutId);
}

QSet<QnUuid> QnVideoWallItemAccessProvider::accessibleLayouts() const
{
    QSet<QnUuid> result;
    for (const auto& videoWall : qnResPool->getResources<QnVideoWallResource>())
        result += videoWallLayouts(videoWall);
    return result;
}
