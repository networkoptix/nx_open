#include "shared_resource_access_provider.h"

#include <core/resource_access/shared_resources_manager.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>

QnSharedResourceAccessProvider::QnSharedResourceAccessProvider(QObject* parent):
    base_type(parent)
{
    connect(qnSharedResourcesManager, &QnSharedResourcesManager::sharedResourcesChanged, this,
        &QnSharedResourceAccessProvider::handleSharedResourcesChanged);
}

QnSharedResourceAccessProvider::~QnSharedResourceAccessProvider()
{
}

QnAbstractResourceAccessProvider::Source QnSharedResourceAccessProvider::baseSource() const
{
    return Source::shared;
}

bool QnSharedResourceAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource) const
{
    NX_ASSERT(acceptable(subject, resource));
    if (!acceptable(subject, resource))
        return false;

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        if (!layout->isShared())
            return false;
    }
    else if (!isMediaResource(resource))
    {
        return false;
    }

    return qnSharedResourcesManager->sharedResources(subject).contains(resource->getId());
}

void QnSharedResourceAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    base_type::handleResourceAdded(resource);

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout, &QnResource::parentIdChanged, this,
            &QnSharedResourceAccessProvider::updateAccessToResource);
    }
}

void QnSharedResourceAccessProvider::handleSharedResourcesChanged(
    const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& oldValues,
    const QSet<QnUuid>& newValues)
{
    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return;

    auto changed = (newValues | oldValues) - (newValues & oldValues);

    for (auto resource: qnResPool->getResources(changed))
        updateAccess(subject, resource);
}
