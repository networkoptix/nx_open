#include "videowall_resource_access_provider.h"

#include <core/resource_access/resource_access_manager.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

QnVideoWallResourceAccessProvider::QnVideoWallResourceAccessProvider(QObject* parent):
    base_type(parent)
{
}

QnVideoWallResourceAccessProvider::~QnVideoWallResourceAccessProvider()
{
}

QnAbstractResourceAccessProvider::Source QnVideoWallResourceAccessProvider::baseSource() const
{
    return Source::videowall;
}

bool QnVideoWallResourceAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!qnResourceAccessManager->hasGlobalPermission(subject, Qn::GlobalControlVideoWallPermission))
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

void QnVideoWallResourceAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    base_type::handleResourceAdded(resource);

    if (auto videoWall = resource.dynamicCast<QnVideoWallResource>())
    {
        connect(videoWall, &QnVideoWallResource::itemAdded, this,
            &QnVideoWallResourceAccessProvider::handleVideoWallItemAdded);
        connect(videoWall, &QnVideoWallResource::itemChanged, this,
            &QnVideoWallResourceAccessProvider::handleVideoWallItemChanged);
        connect(videoWall, &QnVideoWallResource::itemRemoved, this,
            &QnVideoWallResourceAccessProvider::handleVideoWallItemRemoved);
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
    else if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        connect(user, &QnUserResource::permissionsChanged, this,
            [this, user]
            {
                for (const auto& resource : qnResPool->getResources())
                    updateAccess(user, resource);
            });
    }
}

void QnVideoWallResourceAccessProvider::handleVideoWallItemAdded(
    const QnVideoWallResourcePtr& /*resource*/, const QnVideoWallItem &item)
{
    updateByLayoutId(item.layout);
}

void QnVideoWallResourceAccessProvider::handleVideoWallItemChanged(
    const QnVideoWallResourcePtr& /*resource*/,
    const QnVideoWallItem& oldItem,
    const QnVideoWallItem& item)
{
    if (oldItem.layout == item.layout)
        return;

    updateByLayoutId(oldItem.layout);
    updateByLayoutId(item.layout);
}

void QnVideoWallResourceAccessProvider::handleVideoWallItemRemoved(
    const QnVideoWallResourcePtr& /*resource*/, const QnVideoWallItem &item)
{
    updateByLayoutId(item.layout);
}

void QnVideoWallResourceAccessProvider::updateByLayoutId(const QnUuid& id)
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

QSet<QnUuid> QnVideoWallResourceAccessProvider::accessibleLayouts() const
{
    QSet<QnUuid> result;
    for (const auto& videoWall : qnResPool->getResources<QnVideoWallResource>())
    {
        for (const auto& item : videoWall->items()->getItems())
        {
            if (item.layout.isNull())
                continue;
            result << item.layout;
        }
    }

    return result;
}
