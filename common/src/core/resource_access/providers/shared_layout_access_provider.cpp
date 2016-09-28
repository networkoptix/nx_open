#include "shared_layout_access_provider.h"

#include <core/resource_management/resource_pool.h>

#include <core/resource_access/resource_access_manager.h>

#include <core/resource/layout_resource.h>

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
        NX_ASSERT(layout->isShared());
        if (!layout->isShared())
            continue;

        for (const auto& item : layout->getItems())
        {
            /* Only remote resources with correct id can be accessed. */
            auto id = item.resource.id;
            NX_ASSERT(!id.isNull());
            if (id.isNull())
                continue;

            changedResources << id;
        }
    }

    for (auto resource: qnResPool->getResources(changedResources))
        updateAccess(subject, resource);
}
