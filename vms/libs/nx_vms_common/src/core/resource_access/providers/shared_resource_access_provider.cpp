// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shared_resource_access_provider.h"

#include <core/resource_access/shared_resources_manager.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/layout_resource.h>

#include <nx/utils/log/log.h>
#include <common/common_module.h>

namespace nx::core::access {

SharedResourceAccessProvider::SharedResourceAccessProvider(
    Mode mode,
    QObject* parent)
    :
    base_type(mode, parent)
{
    if (mode == Mode::cached)
    {
        connect(sharedResourcesManager(), &QnSharedResourcesManager::sharedResourcesChanged, this,
            &SharedResourceAccessProvider::handleSharedResourcesChanged);
    }
}

SharedResourceAccessProvider::~SharedResourceAccessProvider()
{
}

Source SharedResourceAccessProvider::baseSource() const
{
    return Source::shared;
}

bool SharedResourceAccessProvider::calculateAccess(const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    GlobalPermissions /*globalPermissions*/) const
{
    NX_ASSERT(acceptable(subject, resource));
    if (!acceptable(subject, resource))
        return false;

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        if (!layout->isShared())
        {
            NX_VERBOSE(QnLog::PERMISSIONS_LOG.join(this), "%1 is not shared, ignore it",
                layout->getName());
            return false;
        }
    }
    else if (!isMediaResource(resource))
    {
        NX_VERBOSE(QnLog::PERMISSIONS_LOG.join(this), "%1 has invalid type, ignore it",
            resource->getName());
        return false;
    }

    bool result = sharedResourcesManager()->hasSharedResource(subject, resource->getId());

    NX_VERBOSE(QnLog::PERMISSIONS_LOG.join(this), "update access %1 to %2: %3",
        subject.name(), resource->getName(), result);

    return result;
}

void SharedResourceAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    base_type::handleResourceAdded(resource);

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout, &QnResource::parentIdChanged, this,
            &SharedResourceAccessProvider::updateAccessToResource);
    }
}

void SharedResourceAccessProvider::handleSharedResourcesChanged(
    const QnResourceAccessSubject& subject,
    const QSet<QnUuid>& oldValues,
    const QSet<QnUuid>& newValues)
{
    NX_ASSERT(mode() == Mode::cached);

    NX_ASSERT(subject.isValid());
    if (!subject.isValid())
        return;

    auto changed = (newValues | oldValues) - (newValues & oldValues);

    QString subjectName = subject.name();

    const auto changedResources = commonModule()->resourcePool()->getResourcesByIds(changed);
    for (const auto& resource: changedResources)
    {
        if (newValues.contains(resource->getId()))
        {
            NX_VERBOSE(QnLog::PERMISSIONS_LOG.join(this), "%1 shared to %2",
                resource->getName(), subjectName);
        }
        else
        {
            NX_VERBOSE(QnLog::PERMISSIONS_LOG.join(this), "%1 no more shared to %2",
                resource->getName(),
                subjectName);
        }
        updateAccess(subject, resource);
    }
}

} // namespace nx::core::access
