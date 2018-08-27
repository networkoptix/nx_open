#pragma once

#include <common/common_globals.h>

#include <nx/core/core_fwd.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/user_access_data.h>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>

#include <utils/common/connective.h>
#include <common/common_module_aware.h>

class QnGlobalPermissionsManager:
    public Connective<QObject>,
    public QnCommonModuleAware
{
    Q_OBJECT

    using base_type = Connective<QObject>;
public:
    QnGlobalPermissionsManager(nx::core::access::Mode mode, QObject* parent);
    virtual ~QnGlobalPermissionsManager();

    /** Get a set of global permissions that will not work without the given one. */
    static GlobalPermissions dependentPermissions(GlobalPermission value);

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

signals:
    void globalPermissionsChanged(const QnResourceAccessSubject& subject,
        GlobalPermissions permissions);

private:
    GlobalPermissions filterDependentPermissions(GlobalPermissions source) const;

    void updateGlobalPermissions(const QnResourceAccessSubject& subject);

    GlobalPermissions calculateGlobalPermissions(const QnResourceAccessSubject& subject) const;

    void setGlobalPermissionsInternal(const QnResourceAccessSubject& subject,
        GlobalPermissions permissions);

    void handleResourceAdded(const QnResourcePtr& resource);
    void handleResourceRemoved(const QnResourcePtr& resource);
    void handleRoleAddedOrUpdated(const nx::vms::api::UserRoleData& userRole);
    void handleRoleRemoved(const nx::vms::api::UserRoleData& userRole);
    void handleSubjectRemoved(const QnResourceAccessSubject& subject);
private:
    const nx::core::access::Mode m_mode;
    mutable QnMutex m_mutex;
    QHash<QnUuid, GlobalPermissions> m_cache;
};
