#include "videowall_resource_access_provider.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>

QnVideoWallResourceAccessProvider::QnVideoWallResourceAccessProvider(QObject* parent):
    base_type(parent)
{
}

QnVideoWallResourceAccessProvider::~QnVideoWallResourceAccessProvider()
{
}

bool QnVideoWallResourceAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{

    QSet<QnUuid> layoutIds;
    for (const auto& videoWall : qnResPool->getResources<QnVideoWallResource>())
    {
        for (const auto& item : videoWall->items()->getItems())
        {
            if (item.layout.isNull())
                continue;
            layoutIds << item.layout;
        }
    }

    auto resourceId = resource->getId();
    if (layoutIds.contains(resourceId))
        return true;
/*
    auto layouts = qnResPool->getResources<QnLayoutResource>(layoutIds);

    for (const auto& layout : layouts)
    {
        for (const auto& item : layout->getItems())
        {
            if (item.resource.id == resourceId)
                return true;
        }
    }
    */

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
        updateAccessToResource(layout);
}
