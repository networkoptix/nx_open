// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/unique_key_source.h>
#include <core/resource_access/resource_access_subject.h>

class QnCommonModule;
class QnResourcePool;
class QnSharedResourcesManager;
class QnGlobalPermissionsManager;
namespace nx::core::access { class ResourceAccessProvider; }

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

class CameraResourceIndex;
class UserLayoutResourceIndex;
class UserRolesProvider;
class WebPageResourceIndex;

class NX_VMS_CLIENT_DESKTOP_API ResourceTreeItemKeySourcePool
{
    using UniqueResourceSourcePtr = entity_item_model::UniqueResourceSourcePtr;
    using UniqueUuidSourcePtr = entity_item_model::UniqueUuidSourcePtr;
    using UniqueStringSourcePtr = entity_item_model::UniqueStringSourcePtr;

public:
    ResourceTreeItemKeySourcePool(
        const QnCommonModule* commonModule,
        const CameraResourceIndex* cameraResourceIndex,
        const UserLayoutResourceIndex* sharedLayoutResourceIndex);

    ~ResourceTreeItemKeySourcePool();

    /**
     * Provides all non-fake servers in the system.
     */
    UniqueResourceSourcePtr serversSource(const QnResourceAccessSubject& accessSubject,
        bool withEdgeServers);

    using ResourceFilter = std::function<bool(const QnResourcePtr&)>;

    /**
     * Provides all device resources stored in the resource pool.
     */
    UniqueResourceSourcePtr allDevicesSource(
        const QnResourceAccessSubject& accessSubject,
        const ResourceFilter& resourceFilter);

    /**
     * Provides all device resources stored in the resource pool.
     */
    UniqueResourceSourcePtr devicesSource(
        const QnResourceAccessSubject& accessSubject,
        const QnMediaServerResourcePtr& parentServer,
        const ResourceFilter& resourceFilter);

    /**
     * Provides device resources stored in the resource pool along with proxied web pages.
     * @todo Mixing devices with web pages better be done little later.
     * @param parentServer If not null only single devices that belong to that server are
     *     provided, whole set of single devices stored in the resource pool otherwise.
     */
    UniqueResourceSourcePtr devicesAndProxiedWebPagesSource(
        const QnMediaServerResourcePtr& parentServer,
        const QnResourceAccessSubject& accessSubject);

    /**
     * @todo Remove.
     */
    UniqueResourceSourcePtr layoutsSource(const QnUserResourcePtr& user);

    /**
     * @todo Remove.
     */
    UniqueResourceSourcePtr allLayoutsSource(const QnUserResourcePtr& user);

    /**
     * @todo Remove.
     */
    UniqueResourceSourcePtr shareableLayoutsSource(const QnUserResourcePtr& user);

    /**
     * Provides layout resources stored in the resource pool which owned by the given user.
     */
    UniqueResourceSourcePtr userLayoutsSource(const QnUserResourcePtr& user);

    /**
     * Provides users resources stored in the resource pool which doesn't belong to any
     * role.
     */
    UniqueResourceSourcePtr usersSource(const QnUserResourcePtr& excludeUser);

    /**
     * Provides users resources stored in the resource pool which belong to the role with
     * given ID.
     */
    UniqueResourceSourcePtr roleUsersSource(const QnUuid& roleId);

    /**
     * Provides devices accessible to the subject with given ID. Makes sense only for custom
     * users, and role user, otherwise it provides no data.
     */
    UniqueResourceSourcePtr userAccessibleDevicesSource(const QnResourceAccessSubject& subject);

    /**
     * @todo Re-implement.
     */
    UniqueResourceSourcePtr sharedAndOwnLayoutsSource(
        const QnResourceAccessSubject& subject);

    /**
     * Provides layouts accessible to the subject with given ID. Makes sense only for custom
     * users, and role user, otherwise it provides no data.
     */
    UniqueResourceSourcePtr directlySharedLayoutsSource(const QnResourceAccessSubject& subject);

    /**
     * Provides web page resources stored in the resource pool.
     */
    UniqueResourceSourcePtr webPagesSource(
        const QnResourceAccessSubject& subject,
        bool includeProxiedWebPages);

    /**
     * Provides all video walls resources stored in the resource pool.
     */
    UniqueResourceSourcePtr videoWallsSource(const QnResourceAccessSubject& subject);

    /**
     * Provides all local file layout resources stored in the resource pool.
     */
    UniqueResourceSourcePtr fileLayoutsSource();

    /**
     * Provides all cloud layout resources.
     */
    UniqueResourceSourcePtr cloudLayoutsSource();

    /**
     * Provides all local media resources stored in the resource pool.
     */
    UniqueResourceSourcePtr localMediaSource();

    /**
     * Provides fake server resources stored in the resource pool.
     */
    UniqueResourceSourcePtr fakeServerResourcesSource();

    /**
     * Provides ids of cloud systems retrieved from the QnCloudSystemsFinder instance.
     */
    UniqueStringSourcePtr cloudSystemsSource();

    /**
     * Provides cameras of the cloud system.
     */
    UniqueResourceSourcePtr cloudSystemCamerasSource(const QString& systemId);

    /**
     * Provides user roles IDs stored in user roles manager.
     */
    UniqueUuidSourcePtr userRolesSource();

private:
    const QnResourcePool* resourcePool() const;
    const nx::core::access::ResourceAccessProvider* accessProvider() const;
    const QnGlobalPermissionsManager* globalPermissionsManager() const;
    const QnSharedResourcesManager* sharedResourcesManager() const;

private:
    const QnCommonModule* m_commonModule;
    const CameraResourceIndex* m_cameraResourceIndex;
    const UserLayoutResourceIndex* m_userLayoutResourceIndex;
    QScopedPointer<UserRolesProvider> m_userRolesProvider;
    QScopedPointer<WebPageResourceIndex> m_webPageResourceIndex;
};

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
