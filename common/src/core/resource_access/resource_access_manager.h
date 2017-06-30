#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <common/common_module_aware.h>

#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/user_access_data.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>

#include <utils/common/connective.h>
#include <utils/common/updatable.h>

class QnResourceAccessManager:
    public Connective<QObject>,
    public QnUpdatable,
    public QnCommonModuleAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnResourceAccessManager(QObject* parent = nullptr);

    /**
    * \param user                      User or role to get global permissions for.
    * \returns                         Global permissions of the given user,
    *                                  adjusted to take dependencies and superuser status into account.
    */
    Qn::GlobalPermissions globalPermissions(const QnResourceAccessSubject& subject) const;

    /**
    * \param user                      User to get global permissions for.
    * \param requiredPermission        Global permission to check.
    * \returns                         Whether actual global permissions include required permission.
    */
    bool hasGlobalPermission(const QnResourceAccessSubject& subject,
        Qn::GlobalPermission requiredPermission) const;

    /**
    * \param accessRights              Access rights descriptor
    * \param requiredPermission        Global permission to check.
    * \returns                         Whether actual global permissions include required permission.
    */
    bool hasGlobalPermission(const Qn::UserAccessData& accessRights,
        Qn::GlobalPermission requiredPermission) const;

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
        return hasGlobalPermission(subject, Qn::GlobalAdminPermission);
    }

    bool canCreateResource  (const QnResourceAccessSubject& subject, const ec2::ApiStorageData& data) const;
    bool canCreateResource  (const QnResourceAccessSubject& subject, const ec2::ApiLayoutData& data) const;
    bool canCreateResource  (const QnResourceAccessSubject& subject, const ec2::ApiUserData& data) const;
    bool canCreateResource  (const QnResourceAccessSubject& subject, const ec2::ApiVideowallData& data) const;
    bool canCreateResource  (const QnResourceAccessSubject& subject, const ec2::ApiWebPageData& data) const;

    bool canCreateStorage   (const QnResourceAccessSubject& subject, const QnUuid& storageParentId) const;
    bool canCreateLayout    (const QnResourceAccessSubject& subject, const QnUuid& layoutParentId) const;
    bool canCreateUser      (const QnResourceAccessSubject& subject,
        Qn::GlobalPermissions targetPermissions, bool isOwner) const;
    bool canCreateUser(const QnResourceAccessSubject& subject, Qn::UserRole role) const;
    bool canCreateVideoWall (const QnResourceAccessSubject& subject) const;
    bool canCreateWebPage   (const QnResourceAccessSubject& subject) const;

    template <typename ResourceSharedPointer, typename ApiDataType>
    bool canModifyResource(const QnResourceAccessSubject& subject,
        const ResourceSharedPointer& target, const ApiDataType& update) const
    {
        /* By default we require only generic permissions to modify resource. */
        Q_UNUSED(update);
        return hasPermission(subject, target, Qn::ReadWriteSavePermission);
    }

    bool canModifyResource(const QnResourceAccessSubject& subject, const QnResourcePtr& target,
        const ec2::ApiStorageData& update) const;
    bool canModifyResource(const QnResourceAccessSubject& subject, const QnResourcePtr& target,
        const ec2::ApiLayoutData& update) const;
    bool canModifyResource(const QnResourceAccessSubject& subject, const QnResourcePtr& target,
        const ec2::ApiUserData& update) const;
    bool canModifyResource(const QnResourceAccessSubject& subject, const QnResourcePtr& target,
        const ec2::ApiVideowallData& update) const;

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

    void setPermissionsInternal(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource, Qn::Permissions permissions);

private:
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

        friend uint qHash(const PermissionKey& key)
        {
            return qHash(key.subjectId) ^ qHash(key.resourceId);
        }
    };

    QHash<PermissionKey, Qn::Permissions> m_permissionsCache;
};
