#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_access_rights_data.h>

#include <nx/utils/singleton.h>

#include <utils/common/connective.h>

class QnResourceAccessManager : public Connective<QObject>, public Singleton<QnResourceAccessManager>
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnResourceAccessManager(QObject* parent = nullptr);

    void reset(const ec2::ApiAccessRightsDataList& accessRights);

    /** List of resources ids, the given user has access to. */
    QSet<QnUuid> accessibleResources(const QnUuid& userId) const;
    void setAccessibleResources(const QnUuid& userId, const QSet<QnUuid>& resources);

    /**
    * \param user                      User to get global permissions for.
    * \returns                         Global permissions of the given user,
    *                                  adjusted to take deprecation and superuser status into account.
    */
    Qn::GlobalPermissions globalPermissions(const QnUserResourcePtr &user) const;

    /**
    * \param user                      User to get global permissions for.
    * \param requiredPermissions       Global permissions to check.
    * \returns                         Whether actual global permissions include required permission.
    */
    bool hasGlobalPermission(const QnUserResourcePtr &user, Qn::GlobalPermission requiredPermission) const;

    /**
    * \param user                      User that should have permissions.
    * \param resource                  Resource to get permissions for.
    * \returns                         Permissions that user have for the given resource.
    */
    Qn::Permissions permissions(const QnUserResourcePtr& user, const QnResourcePtr& resource);

private:
    /**
    * \param permissions               Permission flags containing some deprecated values.
    * \returns                         Permission flags with deprecated values replaced with new ones.
    */
    static Qn::GlobalPermissions undeprecate(Qn::GlobalPermissions permissions);

    /** Clear all cache values, bound to the given resource id. */
    void clearCache(const QnUuid& id);

    /** Fully clear all caches. */
    void clearCache();

    Qn::Permissions calculatePermissions(const QnUserResourcePtr &user, const QnResourcePtr &target) const;

    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr &user, const QnVirtualCameraResourcePtr &camera)   const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr &user, const QnMediaServerResourcePtr &server)     const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr &user, const QnVideoWallResourcePtr &videoWall)    const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr &user, const QnWebPageResourcePtr &webPage)        const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr &user, const QnLayoutResourcePtr &layout)          const;
    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr &user, const QnUserResourcePtr &targetUser)        const;

private:
    /* Cached value of read-only system state. */
    bool m_readOnlyMode;

    QHash<QnUuid, QSet<QnUuid> > m_accessibleResources;
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
