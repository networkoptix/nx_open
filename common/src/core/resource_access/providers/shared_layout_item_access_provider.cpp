#include "shared_layout_item_access_provider.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource_access/shared_resources_manager.h>

#include <core/resource/layout_resource.h>

namespace {

QSet<QnUuid> layoutItems(const QnLayoutResourcePtr& layout)
{
    QSet<QnUuid> result;
    for (const auto& item : layout->getItems())
    {
        /* Only remote resources with correct id can be accessed. */
        auto id = item.resource.id;
        if (id.isNull())
            continue;

        result << id;
    }
    return result;
}

}

QnSharedLayoutItemAccessProvider::QnSharedLayoutItemAccessProvider(QObject* parent):
    base_type(parent)
{
    connect(qnSharedResourcesManager, &QnSharedResourcesManager::sharedResourcesChanged, this,
        &QnSharedLayoutItemAccessProvider::handleSharedResourcesChanged);
}

QnSharedLayoutItemAccessProvider::~QnSharedLayoutItemAccessProvider()
{

}

QnAbstractResourceAccessProvider::Source QnSharedLayoutItemAccessProvider::baseSource() const
{
    return Source::layout;
}

bool QnSharedLayoutItemAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!isMediaResource(resource))
        return false;

    auto sharedLayouts = qnResPool->getResources<QnLayoutResource>(
        qnSharedResourcesManager->sharedResources(subject));

    auto resourceId = resource->getId();
    for (const auto& layout: sharedLayouts)
    {
        NX_ASSERT(layout->isShared());
        if (!layout->isShared())
            continue;

        for (const auto& item : layout->getItems())
        {
            if (item.resource.id == resourceId)
                return true;
        }
    }

    return false;
}

void QnSharedLayoutItemAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    base_type::handleResourceAdded(resource);

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout, &QnResource::parentIdChanged, this,
            [this, layout]
            {
                updateAccessToLayoutItems(layout);
            });

        auto handleItemChanged =
            [this](const QnLayoutResourcePtr& /*layout*/, const QnLayoutItemData& item)
            {
                /* Only remote resources with correct id can be accessed. */
                if (item.resource.id.isNull())
                    return;
                if (auto resource = qnResPool->getResourceById(item.resource.id))
                    updateAccessToResource(resource);
            };
        connect(layout, &QnLayoutResource::itemAdded, this, handleItemChanged);
        connect(layout, &QnLayoutResource::itemRemoved, this, handleItemChanged);

        updateAccessToLayoutItems(layout);
    }
}

void QnSharedLayoutItemAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    base_type::handleResourceRemoved(resource);
    if (auto layout = resource.dynamicCast<QnLayoutResource>())
        updateAccessToLayoutItems(layout);
}

void QnSharedLayoutItemAccessProvider::handleSharedResourcesChanged(
    const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& oldValues,
    const QSet<QnUuid>& newValues)
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return;

    auto changed = (newValues | oldValues) - (newValues & oldValues);

    QSet<QnUuid> changedResources;
    auto sharedLayouts = qnResPool->getResources<QnLayoutResource>(changed);
    for (const auto& layout : sharedLayouts)
    {
        if (!layout->isShared())
            continue;

        changedResources += layoutItems(layout);
    }

    for (auto resource: qnResPool->getResources(changedResources))
        updateAccess(subject, resource);
}

void QnSharedLayoutItemAccessProvider::updateAccessToLayoutItems(const QnLayoutResourcePtr& layout)
{
    for (auto resource : qnResPool->getResources(layoutItems(layout)))
        updateAccessToResource(resource);
}

