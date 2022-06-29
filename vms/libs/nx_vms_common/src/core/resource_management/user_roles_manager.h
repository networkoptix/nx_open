// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/user_role_data.h>
#include <nx/vms/common/system_context_aware.h>

// TODO: Move to approperate namespace with QnUserRolesManager when Permission Management
// refactoring is over.
class NX_VMS_COMMON_API QnPredefinedUserRoles:
    public QObject
{
    Q_OBJECT

public:
    using UserRoleData = nx::vms::api::UserRoleData;
    using UserRoleDataList = nx::vms::api::UserRoleDataList;

    // TODO: Qn::UserRole with all corresponding methods thould be removed.

    // Returns list of predefined user roles.
    static const QList<Qn::UserRole>& enumValues();

    // Returns predefined user role for corresponding pseudo-uuid.
    // For null uuid returns Qn::CustomPermissions.
    // For any other uuid returns Qn::CustomUserRole without checking
    //  if a role with such uuid actually exists.
    static Qn::UserRole enumValue(const QnUuid& id);

    // Returns pseudo-uuid for predefined user role.
    // For Qn::CustomUserRole and Qn::CustomPermissions returns null uuid.
    static QnUuid id(Qn::UserRole userRole);

    // Returns human-readable name of specified user role.
    static QString name(Qn::UserRole userRole);

    // Returns human-readable description of specified user role.
    static QString description(Qn::UserRole userRole);

    // Returns global permission flags for specified user role.
    static GlobalPermissions permissions(Qn::UserRole userRole);

    // Returns pseudo-uuid for predefined user role according to permissions preset.
    static std::optional<QnUuid> presetId(GlobalPermissions permissions);

    // Returns list of predefined user role information structures.
    static UserRoleDataList list();

    // Returns predefined user role information structure if found
    static std::optional<UserRoleData> get(const QnUuid& id);

    // Returns list of ids of predefined admin roles.
    static const QList<QnUuid>& adminIds();
};

class NX_VMS_COMMON_API QnUserRolesManager:
    public QObject,
    public nx::vms::common::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    using UserRoleData = nx::vms::api::UserRoleData;
    using UserRoleDataList = nx::vms::api::UserRoleDataList;

public:
    QnUserRolesManager(nx::vms::common::SystemContext* context, QObject* parent = nullptr);
    virtual ~QnUserRolesManager();

    // Returns list of information structures for all custom user roles.
    UserRoleDataList userRoles() const;

    // Returns list of information structures for custom user roles specified by their uuids as well
    // as thir inherited roles.
    template <class Ids>
    UserRoleDataList userRoles(const Ids& roleIds) const
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        UserRoleDataList result;
        appendUserRoles(&result, roleIds, lock);
        return result;
    }

    // Splits subject id list into valid users and valid role ids (including predefined).
    template<class IDList> //< std::vector, std::set, QVector, QSet and QList are supported.
    void usersAndRoles(const IDList& ids, QnUserResourceList& users, QList<QnUuid>& roles);

    // Checks if there is a custom user role with specified uuid.
    bool hasRole(const QnUuid& id) const;
    bool hasRoles(const std::vector<QnUuid>& ids) const;

    // Returns information structure for custom user role with specified uuid.
    UserRoleData userRole(const QnUuid& id) const;

    // TODO: Remove usages to add multi-role support.
    // Returns human-readable name of specified user role.
    QString userRoleName(const QnUuid& userRoleId);

    // TODO: Remove usages to add multi-role support.
    // Returns human-readable name of user role of specified user.
    QString userRoleName(const QnUserResourcePtr& user) const;

// Slots called by the message processor:

    // Sets new custom user roles handled by this manager.
    void resetUserRoles(const UserRoleDataList& userRoles);

    // Adds or updates custom user role information:
    void addOrUpdateUserRole(const UserRoleData& role);

    // Removes specified custom user role from this manager.
    void removeUserRole(const QnUuid& id);

signals:
    void userRoleAddedOrUpdated(const nx::vms::api::UserRoleData& userRole);
    void userRoleRemoved(const nx::vms::api::UserRoleData& userRole);

private:
    bool isValidRoleId(const QnUuid& id) const; //< This function is not thread-safe.

    template <class Ids>
    void appendUserRoles(
        UserRoleDataList* list, const Ids& roleIds, const nx::MutexLocker& lock) const
    {
        for (const auto& id: roleIds)
        {
            const auto itr = m_roles.find(id);
            if (itr != m_roles.end())
            {
                list->push_back(itr.value());
                appendUserRoles(list, itr.value().parentRoleIds, lock);
            }
        }
    }

private:
    mutable nx::Mutex m_mutex;
    QHash<QnUuid, UserRoleData> m_roles;
};
