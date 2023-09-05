// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_access/access_rights_resolver.h>
#include <core/resource_access/resource_access_details.h>
#include <core/resource_access/user_access_data.h>
#include <nx/core/core_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/system_context_aware.h>
#include <utils/common/updatable.h>

class QnResourceAccessSubject;

namespace nx::vms::api {

struct AnalyticsEngineData;
struct AnalyticsPluginData;
struct CameraData;
struct StorageData;
struct LayoutData;
struct UserData;
struct UserGroupData;
struct VideowallData;
struct WebPageData;

} // namespace nx::vms::api

class NX_VMS_COMMON_API QnResourceAccessManager:
    public QObject,
    public QnUpdatable,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnResourceAccessManager(
        nx::vms::common::SystemContext* context,
        QObject* parent = nullptr);

    // Helper functions to avoid direct GlobalPermissions checking.
    bool hasPowerUserPermissions(const Qn::UserAccessData& accessData) const;
    bool hasPowerUserPermissions(const QnResourceAccessSubject& subject) const;

    // TODO: #vkutin
    // Think whether we need these two versions or one `accessRights(subject, resourceOrGroupId)`
    nx::vms::api::AccessRights accessRights(
        const QnResourceAccessSubject& subject, const QnResourcePtr& resource) const;

    nx::vms::api::AccessRights accessRights(
        const QnResourceAccessSubject& subject, const QnUuid& resourceGroupId) const;

    bool hasAccessRights(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        nx::vms::api::AccessRights requiredAccessRights) const;

    bool hasAccessRights(const QnResourceAccessSubject& subject, const QnUuid& resourceGroupId,
        nx::vms::api::AccessRights requiredAccessRights) const;

    /**
     * @param subject User or role to get global permissions for.
     * @returns Global permissions of the given user, adjusted to take dependencies and
     *     superuser status into account.
     */
    GlobalPermissions globalPermissions(const QnResourceAccessSubject& subject) const;

    /**
     * @param subject User or role to get global permissions for.
     * @param requiredPermission Global permission to check.
     * @returns True if actual global permissions include required permission.
     */
    bool hasGlobalPermission(const QnResourceAccessSubject& subject,
        GlobalPermission requiredPermission) const;

    /**
     * @param accessData Access rights descriptor.
     * @param requiredPermission Global permission to check.
     * @returns True if actual global permissions include required permission.
     */
    bool hasGlobalPermission(const Qn::UserAccessData& accessData,
        GlobalPermission requiredPermission) const;

    /**
     * @param subject User or group that should have permissions.
     * @param targetResource Resource to get permissions for.
     * @returns Permissions that user have for the given resource.
     */
    Qn::Permissions permissions(const QnResourceAccessSubject& subject,
        const QnResourcePtr& targetResource) const;

    /**
     * @param subject User or group that should have permissions.
     * @param targetGroup Group to get permissions for.
     * @returns Permissions that user have for the given group.
     */
    Qn::Permissions permissions(const QnResourceAccessSubject& subject,
        const nx::vms::api::UserGroupData& targetGroup) const;

    /**
     * @param subject User or group that should have permissions.
     * @param targetId Id of a resource or a user group to get permissions for.
     * @returns Permissions that user have for the given resource.
     */
    Qn::Permissions permissions(const QnResourceAccessSubject& subject,
        const QnUuid& targetId) const;

    /**
     * @param subject User or group that should have permissions.
     * @param targetResource Resource to check permissions for.
     * @param requiredPermissions Permission to check.
     * @returns True if actual permissions include required permission.
     */
    bool hasPermission(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& targetResource,
        Qn::Permissions requiredPermissions) const;

    /**
     * @param subject User or group that should have permissions.
     * @param targetGroup Group to get permissions for.
     * @param requiredPermissions Permission to check.
     * @returns True if actual permissions include required permission.
     */
    bool hasPermission(
        const QnResourceAccessSubject& subject,
        const nx::vms::api::UserGroupData& targetGroup,
        Qn::Permissions requiredPermissions) const;

    /**
     * @param subject User or group that should have permissions.
     * @param target Id of a resource or a user group to get permissions for.
     * @param requiredPermissions Permission to check.
     * @returns True if actual permissions include required permission.
     */
    bool hasPermission(
        const QnResourceAccessSubject& subject,
        const QnUuid& targetId,
        Qn::Permissions requiredPermissions) const;

    /**
     * @param accessData Access rights descriptor.
     * @param resource Resource to get permissions for.
     * @param permissions Permission to check.
     * @returns True if actual permissions include required permission.
     */
    bool hasPermission(
        const Qn::UserAccessData& accessData,
        const QnResourcePtr& targetResource,
        Qn::Permissions permissions) const;

    /**
     * Returns all ways in which the specified subject gains specified access right to
     * the specified resource, directly and indirectly.
     */
    nx::core::access::ResourceAccessDetails accessDetails(
        const QnUuid& subjectId,
        const QnResourcePtr& resource,
        nx::vms::api::AccessRight accessRight) const;

    using Notifier = nx::core::access::AccessRightsResolver::Notifier;
    Notifier* createNotifier(QObject* parent = nullptr);

    template <typename ApiDataType>
    bool canCreateResourceFromData(
        const QnResourceAccessSubject& subject,
        const ApiDataType& data) const
    {
        using namespace nx::vms::api;
        NX_ASSERT(!isUpdating());
        if constexpr (std::is_same_v<ApiDataType, LayoutData>)
            return canCreateLayout(subject, data);
        else if constexpr (std::is_same_v<ApiDataType, StorageData>)
            return canCreateStorage(subject, data);
        else if constexpr (std::is_same_v<ApiDataType, UserData>)
            return canCreateUser(subject, data);
        else if constexpr (std::is_same_v<ApiDataType, VideowallData>)
            return canCreateVideoWall(subject, data);
        else if constexpr (std::is_same_v<ApiDataType, WebPageData>)
            return canCreateWebPage(subject, data);
        else
            return hasGlobalPermission(subject, GlobalPermission::powerUser);
    }

    template <typename ResourceSharedPointer, typename ApiDataType>
    bool canModifyResource(
        const QnResourceAccessSubject& subject,
        const ResourceSharedPointer& target,
        const ApiDataType& update) const
    {
        using namespace nx::vms::api;
        NX_ASSERT(!isUpdating());
        if constexpr (std::is_same_v<ApiDataType, LayoutData>)
            return canModifyLayout(subject, target, update);
        else if constexpr (std::is_same_v<ApiDataType, StorageData>)
            return canModifyStorage(subject, target, update);
        else if constexpr (std::is_same_v<ApiDataType, UserData>)
            return canModifyUser(subject, target, update);
        else if constexpr (std::is_same_v<ApiDataType, VideowallData>)
            return canModifyVideoWall(subject, target, update);
        else // By default we require only generic permissions to modify resource.
            return hasPermission(subject, target, Qn::ReadWriteSavePermission);
    }

    bool hasAccessToAllCameras(
        const QnUuid& userId,
        nx::vms::api::AccessRights accessRights) const;

    bool hasAccessToAllCameras(
        const Qn::UserAccessData& userAccessData,
        nx::vms::api::AccessRights) const;

//-------------------------------------------------------------------------------------------------
// Layout

    bool canCreateLayout(
        const QnResourceAccessSubject& subject,
        const nx::vms::api::LayoutData& data) const;
    bool canCreateLayout(
        const QnResourceAccessSubject& subject,
        const QnLayoutResourcePtr& layout) const;
    bool canModifyLayout(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& target,
        const nx::vms::api::LayoutData& update) const;

//-------------------------------------------------------------------------------------------------
// Storage

    bool canCreateStorage(
        const QnResourceAccessSubject& subject,
        const nx::vms::api::StorageData& data) const;
    bool canCreateStorage(
        const QnResourceAccessSubject& subject,
        const QnUuid& storageParentId) const;
    bool canModifyStorage(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& target,
        const nx::vms::api::StorageData& update) const;

//-------------------------------------------------------------------------------------------------
// User

    bool canCreateUser(
        const QnResourceAccessSubject& subject,
        const nx::vms::api::UserData& data) const;
    bool canCreateUser(
        const QnResourceAccessSubject& subject,
        GlobalPermissions targetPermissions,
        const std::vector<QnUuid>& targetGroups) const;
    bool canModifyUser(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& target,
        const nx::vms::api::UserData& update) const;

//-------------------------------------------------------------------------------------------------
// Video wall

    bool canCreateVideoWall(
        const QnResourceAccessSubject& subject,
        const nx::vms::api::VideowallData& data) const;
    bool canCreateVideoWall(const QnResourceAccessSubject& subject) const;
    bool canModifyVideoWall(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& target,
        const nx::vms::api::VideowallData& update) const;

//-------------------------------------------------------------------------------------------------
// WebPage

    bool canCreateWebPage(
        const QnResourceAccessSubject& subject,
        const nx::vms::api::WebPageData& data) const;
    bool canCreateWebPage(const QnResourceAccessSubject& subject) const;

signals:
    void resourceAccessReset();
    void permissionsDependencyChanged(const QnResourceList& resources);

protected:
    virtual void afterUpdate() override;

private:
    bool canCreateResourceInternal(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& target) const;

    nx::vms::api::GlobalPermissions accumulatePermissions(nx::vms::api::GlobalPermissions own,
        const std::vector<QnUuid>& parentGroups) const;

    Qn::Permissions calculatePermissions(const QnResourceAccessSubject& subject,
        const QnResourcePtr& target) const;

    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnVirtualCameraResourcePtr& camera) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnMediaServerResourcePtr& server) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnStorageResourcePtr& storage) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnVideoWallResourcePtr& videoWall) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnWebPageResourcePtr& webPage) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnLayoutResourcePtr& layout) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnUserResourcePtr& targetUser) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const nx::vms::api::UserGroupData& targetGroup) const;

    void handleResourceUpdated(const QnResourcePtr& resource);
    void handleResourcesAdded(const QnResourceList& resources);
    void handleResourcesRemoved(const QnResourceList& resources);

private:
    const std::unique_ptr<nx::core::access::AccessRightsResolver> m_accessRightsResolver;
    QSet<QnResourcePtr> m_updatingResources;
    mutable nx::Mutex m_updatingResourcesMutex;
};
