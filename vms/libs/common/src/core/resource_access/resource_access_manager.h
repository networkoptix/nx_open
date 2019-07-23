#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <common/common_module_aware.h>

#include <nx/core/core_fwd.h>
#include <core/resource_access/user_access_data.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>

#include <utils/common/connective.h>
#include <utils/common/updatable.h>

class QnResourceAccessSubject;

class QnResourceAccessManager:
    public Connective<QObject>,
    public QnUpdatable,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnResourceAccessManager(nx::core::access::Mode mode, QObject* parent = nullptr);

    /**
    * \param user                      User or role to get global permissions for.
    * \returns                         Global permissions of the given user,
    *                                  adjusted to take dependencies and superuser status into account.
    */
    GlobalPermissions globalPermissions(const QnResourceAccessSubject& subject) const;

    /**
    * \param user                      User to get global permissions for.
    * \param requiredPermission        Global permission to check.
    * \returns                         Whether actual global permissions include required permission.
    */
    bool hasGlobalPermission(const QnResourceAccessSubject& subject,
        GlobalPermission requiredPermission) const;

    /**
    * \param accessRights              Access rights descriptor
    * \param requiredPermission        Global permission to check.
    * \returns                         Whether actual global permissions include required permission.
    */
    bool hasGlobalPermission(const Qn::UserAccessData& accessRights,
        GlobalPermission requiredPermission) const;

    /**
    * \param user                      User that should have permissions.
    * \param resource                  Resource to get permissions for.
    * \returns                         Permissions that user have for the given resource.
    */
    Qn::Permissions permissions(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;

    /**
    * \param user                      User that should have permissions.
    * \param resource                  Resource to get permissions for.
    * \param requiredPermission        Permission to check.
    * \returns                         Whether actual permissions include required permission.
    */
    bool hasPermission(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        Qn::Permissions requiredPermissions) const;

    /**
    * \param accessRights              access rights descriptor.
    * \param resource                  Resource to get permissions for.
    * \param requiredPermission        Permission to check.
    * \returns                         Whether actual permissions include required permission.
    */
    bool hasPermission(
        const Qn::UserAccessData& accessRights,
        const QnResourcePtr& resource,
        Qn::Permissions permissions) const;

    /**
    * \param user                      User that should have permissions for resource creating.
    * \param resource                  Resource to get permissions for.
    * \returns                         Whether user can create this resource.
    */
    bool canCreateResource(const QnResourceAccessSubject& subject, const QnResourcePtr& target) const;

    template <typename ApiDataType>
    bool canCreateResource(const QnResourceAccessSubject& subject, const ApiDataType& /*data*/) const
    {
        return hasGlobalPermission(subject, GlobalPermission::admin);
    }

    bool canCreateResource(
        const QnResourceAccessSubject& subject,
        const nx::vms::api::StorageData& data) const;
    bool canCreateResource(
        const QnResourceAccessSubject& subject,
        const nx::vms::api::LayoutData& data) const;
    bool canCreateResource(
        const QnResourceAccessSubject& subject,
        const nx::vms::api::UserData& data) const;
    bool canCreateResource(
        const QnResourceAccessSubject& subject,
        const nx::vms::api::VideowallData& data) const;
    bool canCreateResource(
        const QnResourceAccessSubject& subject,
        const nx::vms::api::WebPageData& data) const;
    bool canCreateStorage(
        const QnResourceAccessSubject& subject,
        const QnUuid& storageParentId) const;
    bool canCreateLayout(
        const QnResourceAccessSubject& subject,
        const QnUuid& layoutParentId) const;
    bool canCreateUser(
        const QnResourceAccessSubject& subject,
        GlobalPermissions targetPermissions,
        bool isOwner) const;
    bool canCreateUser(const QnResourceAccessSubject& subject, Qn::UserRole role) const;
    bool canCreateVideoWall(const QnResourceAccessSubject& subject) const;
    bool canCreateWebPage(const QnResourceAccessSubject& subject) const;

    template <typename ResourceSharedPointer, typename ApiDataType>
    bool canModifyResource(const QnResourceAccessSubject& subject,
        const ResourceSharedPointer& target, const ApiDataType& /*update*/) const
    {
        /* By default we require only generic permissions to modify resource. */
        return hasPermission(subject, target, Qn::ReadWriteSavePermission);
    }

    bool canModifyResource(const QnResourceAccessSubject& subject, const QnResourcePtr& target,
        const nx::vms::api::StorageData& update) const;
    bool canModifyResource(const QnResourceAccessSubject& subject, const QnResourcePtr& target,
        const nx::vms::api::LayoutData& update) const;
    bool canModifyResource(const QnResourceAccessSubject& subject, const QnResourcePtr& target,
        const nx::vms::api::UserData& update) const;
    bool canModifyResource(const QnResourceAccessSubject& subject, const QnResourcePtr& target,
        const nx::vms::api::VideowallData& update) const;

signals:
    void permissionsChanged(const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
        Qn::Permissions permissions);

protected:
    virtual void beforeUpdate() override;
    virtual void afterUpdate() override;

private:
    void recalculateAllPermissions();

    void updatePermissions(const QnResourceAccessSubject& subject, const QnResourcePtr& target);
    void updatePermissionsToResource(const QnResourcePtr& resource);
    void updatePermissionsBySubject(const QnResourceAccessSubject& subject);

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);
    void handleSubjectRemoved(const QnResourceAccessSubject& subject);

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

    mutable QnMutex m_mutex;

    struct PermissionKey
    {
        QnUuid subjectId;
        QnUuid resourceId;
        PermissionKey() {}
        PermissionKey(const QnUuid& subjectId, const QnUuid& resourceId) :
            subjectId(subjectId), resourceId(resourceId) {}

        bool operator==(const PermissionKey& other) const
        {
            return subjectId == other.subjectId && resourceId == other.resourceId;
        }

        bool operator<(const PermissionKey& other) const
        {
            if (subjectId != other.subjectId)
                return subjectId < other.subjectId;
            return resourceId < other.resourceId;
        }

        friend uint qHash(const PermissionKey& key)
        {
            return qHash(key.subjectId) ^ qHash(key.resourceId);
        }
    };

    QMap<PermissionKey, Qn::Permissions> m_permissionsCache;
};
