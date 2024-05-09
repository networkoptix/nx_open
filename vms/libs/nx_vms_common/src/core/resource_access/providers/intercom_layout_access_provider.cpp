// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "intercom_layout_access_provider.h"

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/system_context.h>

namespace nx::core::access {

IntercomLayoutAccessProvider::IntercomLayoutAccessProvider(
    Mode mode,
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(mode, context, parent)
{
    if (mode == Mode::cached)
    {
        NX_ASSERT(resourceAccessProvider(), "Initialization order fiasko");

        connect(
            resourceAccessProvider(),
            &AbstractResourceAccessProvider::accessChanged,
            this,
            [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource)
            {
                if (!nx::vms::common::isIntercom(resource))
                    return;

                const auto intercomLayout = resourcePool()->getResourceById(
                    nx::vms::common::calculateIntercomLayoutId(resource));

                updateAccess(subject, intercomLayout);
            });
    }
}

IntercomLayoutAccessProvider::~IntercomLayoutAccessProvider()
{
}

Source IntercomLayoutAccessProvider::baseSource() const
{
    return Source::intercom;
}

bool IntercomLayoutAccessProvider::calculateAccess(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    GlobalPermissions globalPermissions,
    const std::vector<QnUuid>& /*effectiveIds*/) const
{
    if (!resource->hasFlags(Qn::layout))
        return false;

    if (!nx::vms::common::isIntercomLayout(resource))
        return false;

    const auto intercom = resource->getParentResource();
    return NX_ASSERT(intercom) && resourceAccessProvider()->hasAccess(subject, intercom);
}

void IntercomLayoutAccessProvider::fillProviders(
    const QnResourceAccessSubject& subject,
    const QnResourcePtr& resource,
    QnResourceList& providers) const
{
    if (nx::vms::common::isIntercomLayout(resource))
    {
        const auto intercom = resource->getParentResource();
        NX_ASSERT(intercom);
        providers.push_back(intercom);
    }
}

void IntercomLayoutAccessProvider::handleResourceAdded(const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    base_type::handleResourceAdded(resource);

    if (nx::vms::common::isIntercom(resource))
    {
        const auto layout = resourcePool()->getResourceById(
            nx::vms::common::calculateIntercomLayoutId(resource));

        if (layout)
            updateAccessToResource(layout);
    }
    else if (nx::vms::common::isIntercomLayout(resource))
    {
        updateAccessToResource(resource);
    }
}

void IntercomLayoutAccessProvider::handleResourceRemoved(const QnResourcePtr& resource)
{
    NX_ASSERT(mode() == Mode::cached);

    base_type::handleResourceRemoved(resource);

    if (nx::vms::common::isIntercom(resource))
    {
        const auto layout = resourcePool()->getResourceById(
            nx::vms::common::calculateIntercomLayoutId(resource));

        if (layout)
            updateAccessToResource(layout);
    }
}

} // namespace nx::core::access
