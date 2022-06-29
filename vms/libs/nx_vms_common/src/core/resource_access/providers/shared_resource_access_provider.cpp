// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shared_resource_access_provider.h"

#include <core/resource/layout_resource.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/vms/common/system_context.h>

namespace nx::core::access {

SharedResourceAccessProvider::SharedResourceAccessProvider(
    Mode mode,
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(mode, context, parent)
{
    if (mode == Mode::cached)
    {
        connect(m_context->sharedResourcesManager(),
            &QnSharedResourcesManager::sharedResourcesChanged,
            this,
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
            NX_VERBOSE(this, "%1 is not shared, ignore it",
                layout->getName());
            return false;
        }
    }
    else if (!isMediaResource(resource))
    {
        NX_VERBOSE(this, "%1 has invalid type, ignore it",
            resource->getName());
        return false;
    }

    bool result =
        m_context->sharedResourcesManager()->hasSharedResource(subject, resource->getId());

    NX_VERBOSE(this, "update access %1 to %2: %3",
        subject, resource->getName(), result);

    return result;
}

void SharedResourceAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    base_type::handleResourceAdded(resource);

    if (auto layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout.get(), &QnResource::parentIdChanged, this,
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

    const auto changedResources = m_context->resourcePool()->getResourcesByIds(changed);
    for (const auto& resource: changedResources)
    {
        if (newValues.contains(resource->getId()))
        {
            NX_VERBOSE(this, "%1 shared to %2", resource->getName(), subject);
        }
        else
        {
            NX_VERBOSE(this, "%1 no more shared to %2", resource->getName(), subject);
        }
        updateAccess(subject, resource);
    }
}

} // namespace nx::core::access
