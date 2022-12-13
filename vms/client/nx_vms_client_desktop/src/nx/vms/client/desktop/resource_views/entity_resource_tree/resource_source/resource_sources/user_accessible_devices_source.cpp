// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_accessible_devices_source.h"

#include <core/resource/resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/types/access_rights_types.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::core::access;
using namespace nx::vms::api;

UserAccessibleDevicesSource::UserAccessibleDevicesSource(
    const QnResourcePool* resourcePool,
    const QnGlobalPermissionsManager* globalPermissionsManager,
    const ResourceAccessProvider* accessProvider,
    const QnResourceAccessSubject& accessSubject)
    :
    base_type(),
    m_resourcePool(resourcePool),
    m_globalPermissionsManager(globalPermissionsManager),
    m_accessProvider(accessProvider),
    m_accessSubject(accessSubject)
{
    if (m_globalPermissionsManager->hasGlobalPermission(
        m_accessSubject, GlobalPermission::accessAllMedia))
    {
        NX_ASSERT(false, "Source is applied to the wrong type of user with no effect");
        return;
    }

    m_accessProviderConnection = connect(
        m_accessProvider, &ResourceAccessProvider::accessChanged, this,
        &UserAccessibleDevicesSource::onResourceAccessChanged);
}

QVector<QnResourcePtr> UserAccessibleDevicesSource::getResources()
{
    if (m_globalPermissionsManager->hasGlobalPermission(
        m_accessSubject, GlobalPermission::accessAllMedia))
    {
        NX_ASSERT(false, "Such query is unexpected");
        return {};
    }

    const auto resources = m_resourcePool->getResourcesByIds(
        m_accessProvider->accessibleResources(m_accessSubject));

    QVector<QnResourcePtr> result;
    std::copy_if(std::begin(resources), std::end(resources), std::back_inserter(result),
        [this](const QnResourcePtr& resource) { return filter(resource); });

    return result;
}

bool UserAccessibleDevicesSource::filter(const QnResourcePtr& resource) const
{
    const auto resourceFlags = resource->flags();

    static constexpr unsigned kExcludeMask =
        Qn::user | Qn::layout | Qn::videowall | Qn::desktop_camera;
    if ((resourceFlags & kExcludeMask) != 0)
        return false;

    if (resourceFlags.testFlag(Qn::server) && !resourceFlags.testFlag(Qn::fake)
        && (m_accessProvider->accessibleVia(m_accessSubject, resource) != Source::shared))
    {
        return false;
    }

    return true;
}

void UserAccessibleDevicesSource::onResourceAccessChanged(
    const QnResourceAccessSubject& subject, const QnResourcePtr& resource, Source source)
{
    if (m_accessSubject != subject)
        return;

    if (source == Source::permissions)
        return;

    if (source == Source::none)
        emit resourceRemoved(resource);
    else if (filter(resource))
        emit resourceAdded(resource);
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
