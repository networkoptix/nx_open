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

private:
    QHash<QnUuid, QSet<QnUuid> > m_accessibleResources;
    mutable QHash<QnUuid, Qn::GlobalPermissions> m_globalPermissionsCache;
};

#define qnResourceAccessManager QnResourceAccessManager::instance()
