#pragma once

#include <ui/models/user_roles_model.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnUserRolesModel;

struct RoleDescription
{
    RoleDescription() {}

    explicit RoleDescription(const Qn::UserRole roleType);
    explicit RoleDescription(const ec2::ApiUserGroupData& userRole);

    Qn::UserRole roleType;
    QString name;
    QString description;
    Qn::GlobalPermissions permissions;
    QnUuid roleUuid;
};

class QnUserRolesModelPrivate: public Connective<QObject>, public QnWorkbenchContextAware
{
    using base_type = Connective<QObject>;
public:
    QnUserRolesModelPrivate(QnUserRolesModel* parent, QnUserRolesModel::DisplayRoleFlags flags);

    int rowForUser(const QnUserResourcePtr& user) const;

    void setUserRoles(ec2::ApiUserGroupDataList value);

    RoleDescription roleByRow(int row) const;
    int count() const;

    void setCustomRoleStrings(const QString& name, const QString& description);

private:
    void updateStandardRoles();

    bool updateUserRole(const ec2::ApiUserGroupData& userRole);
    bool removeUserRole(const ec2::ApiUserGroupData& userRole);
    bool removeUserRoleById(const QnUuid& roleId);

private:
    QnUserRolesModel* q_ptr;
    Q_DECLARE_PUBLIC(QnUserRolesModel)

    QList<Qn::UserRole> m_standardRoles;
    ec2::ApiUserGroupDataList m_userRoles;
    const bool m_customRoleEnabled;

    QString m_customRoleName;
    QString m_customRoleDescription;
};
