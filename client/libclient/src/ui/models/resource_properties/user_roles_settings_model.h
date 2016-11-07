#pragma once

#include <nx_ec/data/api_user_group_data.h>

#include <ui/models/abstract_permissions_model.h>
#include <ui/workbench/workbench_context_aware.h>

/** Model for editing list of user roles */
class QnUserRolesSettingsModel : public QAbstractListModel, public QnAbstractPermissionsModel
{
    Q_OBJECT

    using base_type = QAbstractListModel;
public:
    struct RoleReplacement
    {
        QnUuid role;
        Qn::GlobalPermissions permissions;
        RoleReplacement();
        RoleReplacement(const QnUuid& role, Qn::GlobalPermissions permissions);
        bool isEmpty() const;
    };

public:
    QnUserRolesSettingsModel(QObject* parent = nullptr);
    virtual ~QnUserRolesSettingsModel();

    ec2::ApiUserGroupDataList roles() const;
    void setRoles(const ec2::ApiUserGroupDataList& value);

    /** Add new role. Returns index of added role in the model. */
    int addRole(const ec2::ApiUserGroupData& role);

    void removeRole(const QnUuid& id, const RoleReplacement& replacement);

    /* Final replacement for a deleted role. */
    RoleReplacement replacement(const QnUuid& source) const;

    /* Direct replacement for a deleted role. May reference a role that is also deleted.
    * Might be needed if we decide to offer custom permissions replacements. */
    RoleReplacement directReplacement(const QnUuid& source) const;

    /** Select role as current. */
    void selectRole(const QnUuid& value);
    QnUuid selectedRole() const;

    /* Following  methods are working with the currently selected role. */

    QString roleName() const;
    void setRoleName(const QString& value);

    virtual Qn::GlobalPermissions rawPermissions() const override;
    virtual void setRawPermissions(Qn::GlobalPermissions value) override;

    virtual QSet<QnUuid> accessibleResources() const override;
    virtual void setAccessibleResources(const QSet<QnUuid>& value) override;

    virtual QnResourceAccessSubject subject() const override;

    /** Get accessible resources for the given role - just for convenience. */
    QSet<QnUuid> accessibleResources(const ec2::ApiUserGroupData& role) const;

    /** Get list of users for given role. */
    QnUserResourceList users(const QnUuid& roleId, bool withCandidates) const;

    /** Get list of users for selected role. */
    QnUserResourceList users(bool withCandidates) const;

    /* Methods of QAbstractItemModel */
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;

private:
    ec2::ApiUserGroupDataList::iterator currentRole();
    ec2::ApiUserGroupDataList::const_iterator currentRole() const;

private:
    QnUuid m_currentRoleId;
    ec2::ApiUserGroupDataList m_roles;
    QHash<QnUuid, QSet<QnUuid>> m_accessibleResources;
    QHash<QnUuid, RoleReplacement> m_replacements;
};
