// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <shared_mutex>

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_access/permissions_cache.h>
#include <core/resource_access/user_access_data.h>
#include <nx/core/core_fwd.h>
#include <nx/vms/common/system_context_aware.h>
#include <utils/common/updatable.h>

class QnResourceAccessSubject;
namespace nx::core::access { class PermissionsCache; }

namespace nx::vms::api {

struct AnalyticsEngineData;
struct AnalyticsPluginData;
struct CameraData;
struct StorageData;
struct LayoutData;
struct UserData;
struct VideowallData;
struct WebPageData;

} // namespace nx::vms::api

class NX_VMS_COMMON_API QnResourceAccessManager:
    public QObject,
    public QnUpdatable,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnResourceAccessManager(
        nx::core::access::Mode mode,
        nx::vms::common::SystemContext* context,
        QObject* parent = nullptr);

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
     * @param accessRights Access rights descriptor.
     * @param requiredPermission Global permission to check.
     * @returns True if actual global permissions include required permission.
     */
    bool hasGlobalPermission(const Qn::UserAccessData& accessRights,
        GlobalPermission requiredPermission) const;

    /**
     * @param subject User that should have permissions.
     * @param resource Resource to get permissions for.
     * @returns Permissions that user have for the given resource.
     */
    Qn::Permissions permissions(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;

    /**
     * @param subject User that should have permissions.
     * @param resource Resource to get permissions for.
     * @param requiredPermissions Permission to check.
     * @returns True if actual permissions include required permission.
     */
    bool hasPermission(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        Qn::Permissions requiredPermissions) const;

    /**
     * @param accessRights Access rights descriptor.
     * @param resource Resource to get permissions for.
     * @param permissions Permission to check.
     * @returns True if actual permissions include required permission.
     */
    bool hasPermission(
        const Qn::UserAccessData& accessRights,
        const QnResourcePtr& resource,
        Qn::Permissions permissions) const;

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
            return hasGlobalPermission(subject, GlobalPermission::admin);
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
        bool isOwner) const;
    bool canCreateUser(
        const QnResourceAccessSubject& subject,
        Qn::UserRole role) const;
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
    void permissionsChanged(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        Qn::Permissions permissions);
    void allPermissionsRecalculated();

protected:
    virtual void beforeUpdate() override;
    virtual void afterUpdate() override;

private:
    bool canCreateResourceInternal(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& target) const;

    void recalculateAllPermissions();

    void updatePermissions(const QnResourceAccessSubject& subject, const QnResourcePtr& target);
    void updatePermissionsToResource(const QnResourcePtr& resource);
    void updatePermissionsBySubject(const QnResourceAccessSubject& subject);

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);
    void handleUserRoleRemoved(const QnResourceAccessSubject& subject);

    Qn::Permissions calculatePermissions(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& target,
        GlobalPermissions* globalPermissionsHint = nullptr,
        bool* hasAccessToResourceHint = nullptr) const;

    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnVirtualCameraResourcePtr& camera,
        GlobalPermissions globalPermissions,
        bool hasAccessToResource) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnMediaServerResourcePtr& server,
        GlobalPermissions globalPermissions,
        bool hasAccessToResource) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnStorageResourcePtr& storage,
        GlobalPermissions globalPermissions,
        bool hasAccessToResource) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnVideoWallResourcePtr& videoWall,
        GlobalPermissions globalPermissions,
        bool hasAccessToResource) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnWebPageResourcePtr& webPage,
        GlobalPermissions globalPermissions,
        bool hasAccessToResource) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnLayoutResourcePtr& layout,
        GlobalPermissions globalPermissions,
        bool hasAccessToResource) const;
    Qn::Permissions calculatePermissionsInternal(const QnResourceAccessSubject& subject,
        const QnUserResourcePtr& targetUser,
        GlobalPermissions globalPermissions,
        bool hasAccessToResource) const;

    void setPermissionsInternal(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource, Qn::Permissions permissions);

private:
    const nx::core::access::Mode m_mode;
    mutable std::shared_mutex m_permissionsCacheMutex;
    std::unique_ptr<nx::core::access::PermissionsCache> m_permissionsCache;
};
