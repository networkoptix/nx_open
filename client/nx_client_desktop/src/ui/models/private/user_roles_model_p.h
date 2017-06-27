#pragma once

#include <QtCore/QSet>
#include <QtCore/QPersistentModelIndex>

#include <ui/models/user_roles_model.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

class QnUserRolesModel;

struct RoleDescription
{
    RoleDescription() {}

    explicit RoleDescription(const Qn::UserRole roleType);
    explicit RoleDescription(const ec2::ApiUserRoleData& userRole);

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
    int rowForRole(Qn::UserRole role) const;

    void setUserRoles(ec2::ApiUserRoleDataList value);

    RoleDescription roleByRow(int row) const;
    int count() const;

    QnUuid id(int row, bool predefinedRoleIdsEnabled) const;

    void setCustomRoleStrings(const QString& name, const QString& description);

private:
    void updateStandardRoles();

    bool updateUserRole(const ec2::ApiUserRoleData& userRole);
    bool removeUserRole(const ec2::ApiUserRoleData& userRole);
    bool removeUserRoleById(const QnUuid& roleId);

private:
    QnUserRolesModel* q_ptr;
    Q_DECLARE_PUBLIC(QnUserRolesModel)

    QList<Qn::UserRole> m_standardRoles;
    ec2::ApiUserRoleDataList m_userRoles;
    const bool m_customRoleEnabled;
    const bool m_onlyAssignable;

    QString m_customRoleName;
    QString m_customRoleDescription;

    bool m_checkable = false;
    bool m_predefinedRoleIdsEnabled = false;
    QSet<QPersistentModelIndex> m_checked;
};
