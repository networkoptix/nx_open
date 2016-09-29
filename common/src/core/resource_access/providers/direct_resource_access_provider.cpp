#include "direct_resource_access_provider.h"

#include <core/resource_access/shared_resources_manager.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>

QnDirectResourceAccessProvider::QnDirectResourceAccessProvider(QObject* parent):
    base_type(parent)
{
    connect(qnSharedResourcesManager, &QnSharedResourcesManager::sharedResourcesChanged, this,
        &QnDirectResourceAccessProvider::handleSharedResourcesChanged);
}

QnDirectResourceAccessProvider::~QnDirectResourceAccessProvider()
{
}

QnAbstractResourceAccessProvider::Source QnDirectResourceAccessProvider::baseSource() const
{
    return Source::direct;
}

bool QnDirectResourceAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
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

void QnDirectResourceAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    base_type::handleResourceAdded(resource);

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout, &QnResource::parentIdChanged, this,
            &QnDirectResourceAccessProvider::updateAccessToResource);
    }
}

void QnDirectResourceAccessProvider::handleSharedResourcesChanged(
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

    for (auto resource: qnResPool->getResources(added + removed))
        updateAccess(subject, resource);
}
