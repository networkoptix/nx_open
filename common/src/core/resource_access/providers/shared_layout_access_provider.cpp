#include "shared_layout_access_provider.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource_access/resource_access_manager.h>

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

QnSharedLayoutAccessProvider::QnSharedLayoutAccessProvider(QObject* parent):
    base_type(parent)
{
    connect(qnResourceAccessManager, &QnResourceAccessManager::accessibleResourcesChanged, this,
        &QnSharedLayoutAccessProvider::handleAccessibleResourcesChanged);
}

QnSharedLayoutAccessProvider::~QnSharedLayoutAccessProvider()
{

}

bool QnSharedLayoutAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    if (!isMediaResource(resource))
        return false;

    auto sharedLayouts = qnResPool->getResources<QnLayoutResource>(
        qnResourceAccessManager->accessibleResources(subject));

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

void QnSharedLayoutAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    base_type::handleResourceAdded(resource);

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout, &QnResource::parentIdChanged, this,
            [this, layout]
            {
                for (auto resource : qnResPool->getResources(layoutItems(layout)))
                    updateAccessToResource(resource);
            });
    }
}

void QnSharedLayoutAccessProvider::handleAccessibleResourcesChanged(
    const QnResourceAccessSubject& subject, const QSet<QnUuid>& resourceIds)
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return;

    auto accessible = this->accessible(subject);
    auto added = resourceIds - accessible;
    auto removed = accessible - resourceIds;

    auto changed = (resourceIds | accessible) - (resourceIds & accessible);
    NX_ASSERT(changed == (added + removed));

    QSet<QnUuid> changedResources;
    auto sharedLayouts = qnResPool->getResources<QnLayoutResource>(added + removed);
    for (const auto& layout : sharedLayouts)
    {
        if (!layout->isShared())
            continue;

        changedResources += layoutItems(layout);
    }

    for (auto resource: qnResPool->getResources(changedResources))
        updateAccess(subject, resource);
}

