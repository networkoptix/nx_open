#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>
#include <nx_ec/data/api_access_rights_data.h>
#include <nx_ec/data/api_user_group_data.h>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>

#include <utils/common/connective.h>

class QnResourceAccessManager : public Connective<QObject>, public Singleton<QnResourceAccessManager>
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnResourceAccessManager(QObject* parent = nullptr);

    void resetAccessibleResources(const ec2::ApiAccessRightsDataList& accessibleResourcesList);

    ec2::ApiUserGroupDataList userGroups() const;
    void resetUserGroups(const ec2::ApiUserGroupDataList& userGroups);

    ec2::ApiUserGroupData userGroup(const QnUuid& groupId) const;
    void addOrUpdateUserGroup(const ec2::ApiUserGroupData& userGroup);
    void removeUserGroup(const QnUuid& groupId);

    /** List of resources ids, the given user has access to. */
    QSet<QnUuid> accessibleResources(const QnUuid& userId) const;
    void setAccessibleResources(const QnUuid& userId, const QSet<QnUuid>& resources);

    /**
    * \param user                      User to get global permissions for.
    * \returns                         Global permissions of the given user,
    *                                  adjusted to take deprecation and superuser status into account.
    */
    Qn::GlobalPermissions globalPermissions(const QnUserResourcePtr& user) const;

    /**
    * \param user                      User to get global permissions for.
    * \param requiredPermission        Global permission to check.
    * \returns                         Whether actual global permissions include required permission.
    */
    bool hasGlobalPermission(const QnUserResourcePtr& user, Qn::GlobalPermission requiredPermission) const;

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
    bool hasPermission(const QnUserResourcePtr& user, const QnResourcePtr& resource, Qn::Permissions requiredPermissions) const;

    /**
    * \param user                      User that should have permissions for resource creating.
    * \param resource                  Resource to get permissions for.
    * \returns                         Whether user can create this resource.
    */
    bool canCreateResource(const QnUserResourcePtr& user, const QnResourcePtr& target) const;

    template <typename ApiDataType>
    bool canCreateResource(const QnUserResourcePtr& /*user*/, const ApiDataType& /*data*/) const
    {
        /* By default we cannot create resources manually. */
        return false;
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

signals:
    void accessibleResourcesChanged(const QnUuid& userId);

    void userGroupAddedOrUpdated(const ec2::ApiUserGroupData& userGroup);
    void userGroupRemoved(const QnUuid& groupId);

private:
    /** Clear all cache values, bound to the given resource. */
    void invalidateResourceCache(const QnResourcePtr& resource);

    Qn::Permissions calculatePermissions(const QnUserResourcePtr& user, const QnResourcePtr& target) const;

    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnVirtualCameraResourcePtr& camera)   const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnMediaServerResourcePtr& server)     const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnStorageResourcePtr& storage)        const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnVideoWallResourcePtr& videoWall)    const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnWebPageResourcePtr& webPage)        const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnLayoutResourcePtr& layout)          const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr& user, const QnUserResourcePtr& targetUser)        const;

    bool isAccessibleResource(const QnUserResourcePtr& user, const QnResourcePtr& resource) const;
private:
    mutable QnMutex m_mutex;

    QHash<QnUuid, QSet<QnUuid> > m_accessibleResources;
    QHash<QnUuid, ec2::ApiUserGroupData> m_userGroups;

    mutable QHash<QnUuid, Qn::GlobalPermissions> m_globalPermissionsCache;

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
};

#define qnResourceAccessManager QnResourceAccessManager::instance()
