#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_subject.h>

#include <nx_ec/data/api_fwd.h>
#include <nx_ec/data/api_access_rights_data.h>


#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>

#include <utils/common/connective.h>
#include "user_access_data.h"

class QnResourceAccessManager : public Connective<QObject>, public Singleton<QnResourceAccessManager>
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnResourceAccessManager(QObject* parent = nullptr);

    /** Get a set of global permissions that will not work without the given one. */
    static Qn::GlobalPermissions dependentPermissions(Qn::GlobalPermission value);

    void resetAccessibleResources(const ec2::ApiAccessRightsDataList& accessibleResourcesList);

    /** List of resources ids, the given user has access to (only given directly). */
    QSet<QnUuid> accessibleResources(const QnResourceAccessSubject& subject) const;
    void setAccessibleResources(const QnResourceAccessSubject& subject, const QSet<QnUuid>& resources);

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
    bool hasGlobalPermission(const QnResourceAccessSubject& subject, Qn::GlobalPermission requiredPermission) const;

    /**
    * \param accessRights              Access rights descriptor
    * \param requiredPermission        Global permission to check.
    * \returns                         Whether actual global permissions include required permission.
    */
    bool hasGlobalPermission(const Qn::UserAccessData& accessRights, Qn::GlobalPermission requiredPermission) const;

    /**
    * \param user                      User that should have permissions.
    * \param resource                  Resource to get permissions for.
    * \returns                         Permissions that user have for the given resource.
    */
    Qn::Permissions permissions(const QnUserResourcePtr& user, const QnResourcePtr& resource) const;

    /**
    * \param user                      User that should have permissions.
    * \param resource                  Resource to get permissions for.
    * \param requiredPermission        Permission to check.
    * \returns                         Whether actual permissions include required permission.
    */
    bool hasPermission(
        const QnUserResourcePtr& user,
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
        const QnResourcePtr& mediaResource,
        Qn::Permissions permissions) const;


    /**
    * \param user                      User that should have permissions for resource creating.
    * \param resource                  Resource to get permissions for.
    * \returns                         Whether user can create this resource.
    */
    bool canCreateResource(const QnUserResourcePtr& user, const QnResourcePtr& target) const;

    template <typename ApiDataType>
    bool canCreateResource(const QnUserResourcePtr& user, const ApiDataType& /*data*/) const
    {
        return hasGlobalPermission(user, Qn::GlobalAdminPermission);
    }

    bool canCreateResource  (const QnUserResourcePtr& user, const ec2::ApiStorageData& data) const;
    bool canCreateResource  (const QnUserResourcePtr& user, const ec2::ApiLayoutData& data) const;
    bool canCreateResource  (const QnUserResourcePtr& user, const ec2::ApiUserData& data) const;
    bool canCreateResource  (const QnUserResourcePtr& user, const ec2::ApiVideowallData& data) const;
    bool canCreateResource  (const QnUserResourcePtr& user, const ec2::ApiWebPageData& data) const;

    bool canCreateStorage   (const QnUserResourcePtr& user, const QnUuid& storageParentId) const;
    bool canCreateLayout    (const QnUserResourcePtr& user, const QnUuid& layoutParentId) const;
    bool canCreateUser      (const QnUserResourcePtr& user, Qn::GlobalPermissions targetPermissions, bool isOwner) const;
    bool canCreateVideoWall (const QnUserResourcePtr& user) const;
    bool canCreateWebPage   (const QnUserResourcePtr& user) const;

    template <typename ResourceSharedPointer, typename ApiDataType>
    bool canModifyResource(const QnUserResourcePtr& user, const ResourceSharedPointer& target, const ApiDataType& update) const
    {
        /* By default we require only generic permissions to modify resource. */
        Q_UNUSED(update);
        return hasPermission(user, target, Qn::ReadWriteSavePermission);
    }

    bool canModifyResource  (const QnUserResourcePtr& user, const QnResourcePtr& target,     const ec2::ApiStorageData& update) const;
    bool canModifyResource  (const QnUserResourcePtr& user, const QnResourcePtr& target,      const ec2::ApiLayoutData& update) const;
    bool canModifyResource  (const QnUserResourcePtr& user, const QnResourcePtr& target,        const ec2::ApiUserData& update) const;
    bool canModifyResource  (const QnUserResourcePtr& user, const QnResourcePtr& target,   const ec2::ApiVideowallData& update) const;

    static const QList<Qn::UserRole>& predefinedRoles();

    static QString userRoleName(Qn::UserRole userRole);
    static QString userRoleDescription(Qn::UserRole userRole);
    static Qn::GlobalPermissions userRolePermissions(Qn::UserRole userRole);

    QString userRoleName(const QnUserResourcePtr& user) const;

    static ec2::ApiPredefinedRoleDataList getPredefinedRoles();

signals:
    void accessibleResourcesChanged(const QnResourceAccessSubject& subject);

    /** Notify listeners that permissions possibly changed (not necessarily). */
    void permissionsInvalidated(const QSet<QnUuid>& resourceIds);

private:
    /** Clear all cache values, bound to the given resource. */
    void invalidateResourceCache(const QnResourcePtr& resource);
    void invalidateResourceCacheInternal(const QnUuid& resourceId);
    void invalidateCacheForLayoutItem(const QnLayoutResourcePtr& layout, const QnLayoutItemData& item);
    void invalidateCacheForLayoutItems(const QnResourcePtr& resource);
    void invalidateCacheForVideowallItem(const QnVideoWallResourcePtr &resource, const QnVideoWallItem &item);

    Qn::Permissions calculatePermissions(const QnUserResourcePtr& user, const QnResourcePtr& target) const;

    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnVirtualCameraResourcePtr& camera)   const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnMediaServerResourcePtr& server)     const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnStorageResourcePtr& storage)        const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnVideoWallResourcePtr& videoWall)    const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnWebPageResourcePtr& webPage)        const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnLayoutResourcePtr& layout)          const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnUserResourcePtr& targetUser)        const;

    Qn::GlobalPermissions filterDependentPermissions(Qn::GlobalPermissions source) const;

    void beginUpdateCache();
    void endUpdateCache();

private:
    mutable QnMutex m_mutex;

    QHash<QnUuid, QSet<QnUuid> > m_accessibleResources;

    mutable QHash<QnUuid, Qn::GlobalPermissions> m_globalPermissionsCache;

    /** Counter which is used to avoid repeating signals during one update process.  */
    QAtomicInt m_cacheUpdateCounter;

    /** Set of resources that were invalidated during current update process. */
    QSet<QnUuid> m_invalidatedResources;

    struct PermissionKey
    {
        QnUuid userId;
        QnUuid resourceId;
        PermissionKey() {}
        PermissionKey(const QnUuid& userId, const QnUuid& resourceId) :
            userId(userId), resourceId(resourceId) {}

        bool operator==(const PermissionKey& other) const
        {
            return userId == other.userId && resourceId == other.resourceId;
        }

        friend uint qHash(const PermissionKey& key)
        {
            return qHash(key.userId) ^ qHash(key.resourceId);
        }
    };

    mutable QHash<PermissionKey, Qn::Permissions> m_permissionsCache;

    class UpdateCacheGuard;
};

#define qnResourceAccessManager QnResourceAccessManager::instance()
