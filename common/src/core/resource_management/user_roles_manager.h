#pragma once

#include <nx_ec/data/api_user_group_data.h>

#include <nx/utils/singleton.h>
#include <nx/utils/thread/mutex.h>

class QnUserRolesManager: public QObject, public Singleton<QnUserRolesManager>
{
    Q_OBJECT

    using base_type = QObject;
public:
    QnUserRolesManager(QObject* parent = nullptr);
    virtual ~QnUserRolesManager();

    ec2::ApiUserGroupDataList userRoles() const;
    void resetUserRoles(const ec2::ApiUserGroupDataList& userRoles);

    template <class IDList>
    ec2::ApiUserGroupDataList userRoles(IDList idList) const
    {
        QnMutexLocker lk(&m_mutex);
        ec2::ApiUserGroupDataList result;
        for (const auto& id : idList)
        {
            const auto itr = m_roles.find(id);
            if (itr != m_roles.end())
                result.push_back(itr.value());
        }
        return result;
    }

    bool hasRole(const QnUuid& id) const;
    ec2::ApiUserGroupData userRole(const QnUuid& id) const;
    void addOrUpdateUserRole(const ec2::ApiUserGroupData& role);
    void removeUserRole(const QnUuid& id);

signals:
    void userRoleAddedOrUpdated(const ec2::ApiUserGroupData& userGroup);
    void userRoleRemoved(const QnUuid& groupId);

private:
    mutable QnMutex m_mutex;

    QHash<QnUuid, ec2::ApiUserGroupData> m_roles;
};

#define qnUserRolesManager QnUserRolesManager::instance()
