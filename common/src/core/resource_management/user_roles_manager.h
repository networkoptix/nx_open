#pragma once

#include <common/common_module_aware.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_user_role_data.h>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>

class QnUserRolesManager: public QObject, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnUserRolesManager(QObject* parent);
    virtual ~QnUserRolesManager();

    // Returns list of information structures for all custom user roles.
    ec2::ApiUserRoleDataList userRoles() const;

    // Returns list of information structures for custom user roles specified by their uuids.
    template <class IDList>
    ec2::ApiUserRoleDataList userRoles(IDList idList) const
    {
        QnMutexLocker lk(&m_mutex);
        ec2::ApiUserRoleDataList result;
        for (const auto& id : idList)
        {
            const auto itr = m_roles.find(id);
            if (itr != m_roles.end())
                result.push_back(itr.value());
        }
        return result;
    }

    // Splits subject id list into valid users and valid role ids (including predefined).
    template<class IDList> //< std::vector, std::set, QVector, QSet and QList are supported.
    void usersAndRoles(const IDList& ids, QnUserResourceList& users, QList<QnUuid>& roles);

    // Checks if there is a custom user role with specified uuid.
    bool hasRole(const QnUuid& id) const;

    // Returns information structure for custom user role with specified uuid.
    ec2::ApiUserRoleData userRole(const QnUuid& id) const;

    // Returns custom or predefined role id for specified user.
    static QnUuid unifiedUserRoleId(const QnUserResourcePtr& user);

    // Returns list of predefined user roles.
    static const QList<Qn::UserRole>& predefinedRoles();

    // Returns human-readable description of specified user role.
    static QString userRoleDescription(Qn::UserRole userRole);

    // Returns global permission flags for specified user role.
    static Qn::GlobalPermissions userRolePermissions(Qn::UserRole userRole);

    // Returns human-readable name of specified user role.
    static QString userRoleName(Qn::UserRole userRole);

    // Returns human-readable name of specified user role.
    QString userRoleName(const QnUuid& userRoleId);

    // Returns human-readable name of user role of specified user.
    QString userRoleName(const QnUserResourcePtr& user) const;

    // Returns pseudo-uuid for predefined user role.
    // For Qn::CustomUserRole and Qn::CustomPermissions returns null uuid.
    static QnUuid predefinedRoleId(Qn::UserRole userRole);

    // Returns list of ids of predefined admin roles.
    static const QList<QnUuid>& adminRoleIds();

    // Returns predefined user role for corresponding pseudo-uuid.
    // For null uuid returns Qn::CustomPermissions.
    // For any other uuid returns Qn::CustomUserRole without checking
    //  if a role with such uuid actually exists.
    static Qn::UserRole predefinedRole(const QnUuid& id);

    // Returns list of predefined user role information structures.
    static ec2::ApiPredefinedRoleDataList getPredefinedRoles();

// Slots called by the message processor:

    // Sets new custom user roles handled by this manager.
    void resetUserRoles(const ec2::ApiUserRoleDataList& userRoles);

    // Adds or updates custom user role information:
    void addOrUpdateUserRole(const ec2::ApiUserRoleData& role);

    // Removes specified custom user role from this manager.
    void removeUserRole(const QnUuid& id);

signals:
    void userRoleAddedOrUpdated(const ec2::ApiUserRoleData& userRole);
    void userRoleRemoved(const ec2::ApiUserRoleData& userRole);

private:
    bool isValidRoleId(const QnUuid& id) const; //< This function is not thread-safe.

private:
    mutable QnMutex m_mutex;
    QHash<QnUuid, ec2::ApiUserRoleData> m_roles;
};
