// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_item_key_source_pool.h"

#include <common/common_module.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/desktop/cross_system/resource_tree/cloud_layouts_source.h>
#include <nx/vms/client/desktop/cross_system/resource_tree/cloud_system_cameras_source.h>
#include <nx/vms/client/desktop/cross_system/resource_tree/cloud_systems_source.h>
#include <nx/vms/client/desktop/system_context.h>

#include "../camera_resource_index.h"
#include "../user_layout_resource_index.h"
#include "../webpage_resource_index.h"
#include "abstract_resource_source.h"
#include "accessible_resource_proxy_source.h"
#include "composite_resource_source.h"
#include "filtered_resource_proxy_source.h"
#include "parent_servers_proxy_source.h"
#include "resource_sources/all_resource_key_sources.h"
#include "user_roles_provider.h"

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::core::access;
using namespace nx::vms::client::desktop::entity_item_model;

ResourceTreeItemKeySourcePool::ResourceTreeItemKeySourcePool(
    const QnCommonModule* commonModule,
    const CameraResourceIndex* cameraResourceIndex,
    const UserLayoutResourceIndex* sharedLayoutResourceIndex)
    :
    m_commonModule(commonModule),
    m_cameraResourceIndex(cameraResourceIndex),
    m_userLayoutResourceIndex(sharedLayoutResourceIndex),
    m_userRolesProvider(new UserRolesProvider(commonModule->userRolesManager())),
    m_webPageResourceIndex(new WebPageResourceIndex(resourcePool()))
{
}

ResourceTreeItemKeySourcePool::~ResourceTreeItemKeySourcePool()
{
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::serversSource(
    const QnResourceAccessSubject& subject,
    bool withEdgeServers)
{
    const bool isCustomUserOrCustomRoleUser = subject.isUser()
        && (subject.user()->userRole() == Qn::UserRole::customPermissions
            || subject.user()->userRole() == Qn::UserRole::customUserRole);

    // Non-custom users are able to see any server in the resource tree.
    if (!isCustomUserOrCustomRoleUser)
    {
        return std::make_shared<ResourceSourceAdapter>(
            std::move(std::make_unique<ServerResourceSource>(resourcePool(), withEdgeServers)));
    }

    // Custom users are able to see only servers which implicitly marked as accessible or parent
    // servers of accessible resources.
    auto accessibleServersSource = std::make_unique<AccessibleResourceProxySource>(
        m_commonModule,
        accessProvider(),
        subject,
        std::make_unique<ServerResourceSource>(resourcePool(), withEdgeServers));

    auto accessibleCamerasSource = std::make_unique<AccessibleResourceProxySource>(
        m_commonModule,
        accessProvider(),
        subject,
        std::make_unique<CameraResourceSource>(m_cameraResourceIndex, QnMediaServerResourcePtr()));

    auto accessibleProxiedWebPagesSource = std::make_unique<AccessibleResourceProxySource>(
        m_commonModule,
        accessProvider(),
        subject,
        std::make_unique<ProxiedWebResourceSource>(
            m_webPageResourceIndex.get(),
            QnMediaServerResourcePtr()));

    auto accessibleResourcesParentServersSource = std::make_unique<ParentServersProxySource>(
        std::make_unique<CompositeResourceSource>(
            std::move(accessibleCamerasSource), std::move(accessibleProxiedWebPagesSource)));

    return std::make_shared<ResourceSourceAdapter>(std::make_unique<CompositeResourceSource>(
        std::move(accessibleServersSource), std::move(accessibleResourcesParentServersSource)));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::allDevicesSource(
    const QnResourceAccessSubject& accessSubject,
    const ResourceFilter& resourceFilter)
{
    return devicesSource(accessSubject, QnMediaServerResourcePtr(), resourceFilter);
}

ResourceTreeItemKeySourcePool::UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::devicesSource(
    const QnResourceAccessSubject& accessSubject,
    const QnMediaServerResourcePtr& parentServer,
    const ResourceFilter& resourceFilter)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<AccessibleResourceProxySource>(
            m_commonModule,
            accessProvider(),
            accessSubject,
            std::make_unique<FilteredResourceProxySource>(
                resourceFilter,
                std::make_unique<CameraResourceSource>(m_cameraResourceIndex, parentServer))));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::devicesAndProxiedWebPagesSource(
    const QnMediaServerResourcePtr& parentServer,
    const QnResourceAccessSubject& subject)
{
    auto cameras = std::make_unique<AccessibleResourceProxySource>(
        m_commonModule,
        accessProvider(),
        subject,
        std::make_unique<CameraResourceSource>(m_cameraResourceIndex, parentServer));

    auto proxiedWebResources = std::make_unique<AccessibleResourceProxySource>(
        m_commonModule,
        accessProvider(),
        subject,
        std::make_unique<ProxiedWebResourceSource>(m_webPageResourceIndex.get(), parentServer));

    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<CompositeResourceSource>(
            std::move(cameras),
            std::move(proxiedWebResources)));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::layoutsSource(
    const QnUserResourcePtr& user)
{
    auto layouts = std::make_unique<AccessibleResourceProxySource>(
        m_commonModule,
        accessProvider(),
        user,
        std::make_unique<LayoutResourceSource>(resourcePool(), user, true));

    auto intercomLayouts = std::make_unique<AccessibleResourceProxySource>(
        m_commonModule,
        accessProvider(),
        user,
        std::make_unique<IntercomLayoutResourceSource>(resourcePool()));

    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<CompositeResourceSource>(
            std::move(layouts),
            std::move(intercomLayouts)
        ));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::allLayoutsSource(
    const QnUserResourcePtr& user)
{
    return std::make_shared<ResourceSourceAdapter>(std::make_unique<AccessibleResourceProxySource>(
        m_commonModule,
        accessProvider(),
        user,
        std::make_unique<LayoutResourceSource>(resourcePool(), QnUserResourcePtr(), true)));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::shareableLayoutsSource(
    const QnUserResourcePtr& user)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<AccessibleResourceProxySource>(
            m_commonModule,
            accessProvider(),
            user,
            std::make_unique<LayoutResourceSource>(resourcePool(), user, true)));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::userLayoutsSource(
    const QnUserResourcePtr& user)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<UserLayoutsSource>(m_userLayoutResourceIndex, user));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::usersSource(
    const QnUserResourcePtr& excludeUser)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<UserResourceSource>(resourcePool(), excludeUser));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::roleUsersSource(const QnUuid& roleId)
{
    return std::make_shared<ResourceSourceAdapter>(std::make_unique<UserResourceSource>(
        resourcePool(), QnUserResourcePtr(), roleId));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::userAccessibleDevicesSource(
    const QnResourceAccessSubject& subject)
{
    return std::make_shared<ResourceSourceAdapter>(std::make_unique<UserAccessibleDevicesSource>(
        resourcePool(), globalPermissionsManager(), accessProvider(), subject));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::sharedAndOwnLayoutsSource(
    const QnResourceAccessSubject& subject)
{
    auto userLayouts = std::make_unique<UserLayoutsSource>(
        m_userLayoutResourceIndex,
        subject.user());

    auto intercomLayouts = std::make_unique<AccessibleResourceProxySource>(
        m_commonModule,
        accessProvider(),
        subject,
        std::make_unique<IntercomLayoutResourceSource>(resourcePool()));

    auto directlySharedLayouts = std::make_unique<DirectlySharedLayoutsSource>(
        resourcePool(),
        globalPermissionsManager(),
        sharedResourcesManager(),
        subject.user());

    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<CompositeResourceSource>(
            std::move(userLayouts),
            std::make_unique<CompositeResourceSource>(
                std::move(intercomLayouts),
                std::move(directlySharedLayouts))));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::directlySharedLayoutsSource(
    const QnResourceAccessSubject& subject)
{
    return std::make_shared<ResourceSourceAdapter>(std::make_unique<DirectlySharedLayoutsSource>(
        resourcePool(), globalPermissionsManager(), sharedResourcesManager(), subject));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::webPagesSource(
    const QnResourceAccessSubject& subject,
    bool includeProxiedWebPages)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<AccessibleResourceProxySource>(
            m_commonModule,
            accessProvider(),
            subject,
            std::make_unique<WebpageResourceSource>(
                m_webPageResourceIndex.get(),
                includeProxiedWebPages)));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::videoWallsSource(
    const QnResourceAccessSubject& subject)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<AccessibleResourceProxySource>(
            m_commonModule,
            accessProvider(),
            subject,
            std::make_unique<VideowallResourceSource>(resourcePool())));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::fileLayoutsSource()
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<FileLayoutsSource>(resourcePool()));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::cloudLayoutsSource()
{
    return std::make_shared<CloudLayoutsSource>();
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::localMediaSource()
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<LocalMediaSource>(resourcePool()));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::fakeServerResourcesSource()
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<FakeServerResourceSource>(resourcePool()));
}

UniqueStringSourcePtr ResourceTreeItemKeySourcePool::cloudSystemsSource()
{
    return std::make_shared<CloudSystemsSource>();
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::cloudSystemCamerasSource(
    const QString& systemId)
{
    return std::make_shared<CloudSystemCamerasSource>(systemId);
}

const QnResourcePool* ResourceTreeItemKeySourcePool::resourcePool() const
{
    return m_commonModule->resourcePool();
}

const ResourceAccessProvider* ResourceTreeItemKeySourcePool::accessProvider() const
{
    return m_commonModule->resourceAccessProvider();
}

const QnGlobalPermissionsManager* ResourceTreeItemKeySourcePool::globalPermissionsManager() const
{
    return m_commonModule->globalPermissionsManager();
}

const QnSharedResourcesManager* ResourceTreeItemKeySourcePool::sharedResourcesManager() const
{
    return m_commonModule->sharedResourcesManager();
}

UniqueUuidSourcePtr ResourceTreeItemKeySourcePool::userRolesSource()
{
    UniqueUuidSourcePtr result = std::make_shared<UniqueKeySource<QnUuid>>();
    std::weak_ptr<UniqueUuidSource> weakResult = result;

    result->initializeRequest =
        [this, weakResult]()
        {
            weakResult.lock()->setKeysHandler(m_userRolesProvider->getAllRoles());
            m_userRolesProvider->installAddRoleObserver(weakResult.lock()->addKeyHandler);
            m_userRolesProvider->installRemoveRoleObserver(weakResult.lock()->removeKeyHandler);
        };

    return result;
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
