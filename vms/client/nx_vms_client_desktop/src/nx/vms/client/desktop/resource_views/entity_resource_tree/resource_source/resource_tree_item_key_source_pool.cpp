// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_item_key_source_pool.h"

#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_layouts_manager.h>
#include <nx/vms/client/desktop/cross_system/resource_tree/cloud_layouts_source.h>
#include <nx/vms/client/desktop/cross_system/resource_tree/cloud_system_cameras_source.h>
#include <nx/vms/client/desktop/cross_system/resource_tree/cloud_systems_source.h>
#include <nx/vms/client/desktop/ini.h>
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
#include "resource_sources/edge_server_reducer_proxy_source.h"

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace nx::core::access;
using namespace nx::vms::client::desktop::entity_item_model;

ResourceTreeItemKeySourcePool::ResourceTreeItemKeySourcePool(
    SystemContext* systemContext,
    const CameraResourceIndex* cameraResourceIndex,
    const UserLayoutResourceIndex* sharedLayoutResourceIndex)
    :
    SystemContextAware(systemContext),
    m_cameraResourceIndex(cameraResourceIndex),
    m_userLayoutResourceIndex(sharedLayoutResourceIndex),
    m_webPageResourceIndex(new WebPageResourceIndex(resourcePool()))
{
}

ResourceTreeItemKeySourcePool::~ResourceTreeItemKeySourcePool()
{
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::serversSource(
    const QnResourceAccessSubject& subject,
    bool reduceEdgeServers)
{
    auto accessibleServersSource = std::make_unique<AccessibleResourceProxySource>(
        systemContext(),
        subject,
        std::make_unique<ServerResourceSource>(resourcePool()));

    auto accessibleCamerasSource = std::make_unique<AccessibleResourceProxySource>(
        systemContext(),
        subject,
        std::make_unique<CameraResourceSource>(m_cameraResourceIndex, QnMediaServerResourcePtr()));

    auto accessibleProxiedWebPagesSource = std::make_unique<AccessibleResourceProxySource>(
        systemContext(),
        subject,
        std::make_unique<ProxiedWebResourceSource>(
            m_webPageResourceIndex.get(),
            QnMediaServerResourcePtr()));

    auto accessibleResourcesParentServersSource = std::make_unique<ParentServersProxySource>(
        std::make_unique<CompositeResourceSource>(
            std::move(accessibleCamerasSource), std::move(accessibleProxiedWebPagesSource)));

    auto compositeServersSource = std::make_unique<CompositeResourceSource>(
        std::move(accessibleServersSource), std::move(accessibleResourcesParentServersSource));

    if (reduceEdgeServers)
    {
        return std::make_shared<ResourceSourceAdapter>(
            std::make_unique<EdgeServerReducerProxySource>(systemContext(), subject,
                std::move(compositeServersSource)));
    }

    return std::make_shared<ResourceSourceAdapter>(std::move(compositeServersSource));
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
            systemContext(),
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
        systemContext(),
        subject,
        std::make_unique<CameraResourceSource>(m_cameraResourceIndex, parentServer));

    auto proxiedWebResources = std::make_unique<AccessibleResourceProxySource>(
        systemContext(),
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
        systemContext(),
        user,
        std::make_unique<LayoutResourceSource>(resourcePool(), user, true));

    auto intercomLayouts = std::make_unique<AccessibleResourceProxySource>(
        systemContext(),
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
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<AccessibleResourceProxySource>(
            systemContext(),
            user,
            std::make_unique<LayoutResourceSource>(resourcePool(), QnUserResourcePtr(), true)));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::shareableLayoutsSource(
    const QnUserResourcePtr& user)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<AccessibleResourceProxySource>(
            systemContext(),
            user,
            std::make_unique<LayoutResourceSource>(resourcePool(), user, true)));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::integrationsSource(
    const QnResourceAccessSubject& subject,
    bool includeProxiedIntegrations)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<AccessibleResourceProxySource>(
            systemContext(),
            subject,
            std::make_unique<WebpageResourceSource>(
                m_webPageResourceIndex.get(),
                nx::vms::api::WebPageSubtype::clientApi,
                includeProxiedIntegrations)));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::webPagesSource(
    const QnResourceAccessSubject& subject,
    bool includeProxiedWebPages)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<AccessibleResourceProxySource>(
            systemContext(),
            subject,
            std::make_unique<WebpageResourceSource>(
                m_webPageResourceIndex.get(),
                nx::vms::api::WebPageSubtype::none,
                includeProxiedWebPages)));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::webPagesAndIntegrationsSource(
    const QnResourceAccessSubject& subject,
    bool includeProxied)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<AccessibleResourceProxySource>(
            systemContext(),
            subject,
            std::make_unique<WebpageResourceSource>(
                m_webPageResourceIndex.get(),
                /*subtype*/ std::nullopt,
                includeProxied)));
}

UniqueResourceSourcePtr ResourceTreeItemKeySourcePool::videoWallsSource(
    const QnResourceAccessSubject& subject)
{
    return std::make_shared<ResourceSourceAdapter>(
        std::make_unique<AccessibleResourceProxySource>(
            systemContext(),
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

UniqueUuidSourcePtr ResourceTreeItemKeySourcePool::otherServersSource()
{
    return std::make_shared<OtherServersSource>(systemContext()->otherServersManager());
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

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
