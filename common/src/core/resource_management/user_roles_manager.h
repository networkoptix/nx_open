#pragma once

#include <common/common_module_aware.h>

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

    ec2::ApiUserRoleDataList userRoles() const;
    void resetUserRoles(const ec2::ApiUserRoleDataList& userRoles);

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

    bool hasRole(const QnUuid& id) const;
    ec2::ApiUserRoleData userRole(const QnUuid& id) const;
    void addOrUpdateUserRole(const ec2::ApiUserRoleData& role);
    void removeUserRole(const QnUuid& id);

    static const QList<Qn::UserRole>& predefinedRoles();

    static QString userRoleDescription(Qn::UserRole userRole);
    static Qn::GlobalPermissions userRolePermissions(Qn::UserRole userRole);

    static QString userRoleName(Qn::UserRole userRole);
    QString userRoleName(const QnUserResourcePtr& user) const;

    static ec2::ApiPredefinedRoleDataList getPredefinedRoles();

signals:
    void userRoleAddedOrUpdated(const ec2::ApiUserRoleData& userRole);
    void userRoleRemoved(const ec2::ApiUserRoleData& userRole);

private:
    mutable QnMutex m_mutex;

    QHash<QnUuid, ec2::ApiUserRoleData> m_roles;
};
