#pragma once

#include <QtCore/QAbstractListModel>

#include <client_core/connection_context_aware.h>

#include <nx_ec/data/api_user_role_data.h>

#include <ui/models/abstract_permissions_model.h>
#include <ui/workbench/workbench_context_aware.h>

/** Model for editing list of user roles */
class QnUserRolesSettingsModel:
    public QAbstractListModel,
    public QnAbstractPermissionsModel,
    public QnConnectionContextAware
{
    Q_OBJECT

    using base_type = QAbstractListModel;
public:
    struct UserRoleReplacement
    {
        QnUuid userRoleId;
        Qn::GlobalPermissions permissions;
        UserRoleReplacement();
        UserRoleReplacement(const QnUuid& userRoleId, Qn::GlobalPermissions permissions);
        bool isEmpty() const;
    };

public:
    QnUserRolesSettingsModel(QObject* parent = nullptr);
    virtual ~QnUserRolesSettingsModel();

    ec2::ApiUserRoleDataList userRoles() const;
    void setUserRoles(const ec2::ApiUserRoleDataList& value);

    /** Add new role. Returns index of added role in the model. */
    int addUserRole(const ec2::ApiUserRoleData& userRole);

    void removeUserRole(const QnUuid& userRoleId, const UserRoleReplacement& replacement);

    /** Final replacement for a deleted role. */
    UserRoleReplacement replacement(const QnUuid& sourceUserRoleId) const;

    /**
     * Direct replacement for a deleted role. May reference a role that is also deleted.
     * Might be needed if we decide to offer custom permissions replacements.
     */
    UserRoleReplacement directReplacement(const QnUuid& sourceUserRoleId) const;

    /** Select role as current. */
    void selectUserRoleId(const QnUuid& value);

    QnUuid selectedUserRoleId() const;

    // The following methods work with the currently selected role.

    QString userRoleName() const;
    void setUserRoleName(const QString& value);

    bool isUserRoleValid(const ec2::ApiUserRoleData& userRole) const;
    bool isValid() const;

    virtual Qn::GlobalPermissions rawPermissions() const override;
    virtual void setRawPermissions(Qn::GlobalPermissions value) override;

    virtual QSet<QnUuid> accessibleResources() const override;
    virtual void setAccessibleResources(const QSet<QnUuid>& value) override;

    virtual QnResourceAccessSubject subject() const override;

    /** Get accessible resources for the given role - just for convenience. */
    QSet<QnUuid> accessibleResources(const ec2::ApiUserRoleData& userRole) const;

    /** Get list of users for given role. */
    QnUserResourceList users(const QnUuid& userRoleId, bool withCandidates) const;

    /** Get list of users for selected role. */
    QnUserResourceList users(bool withCandidates) const;

    /* Methods of QAbstractItemModel */
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;

private:
    ec2::ApiUserRoleDataList::iterator currentRole();
    ec2::ApiUserRoleDataList::const_iterator currentRole() const;

private:
    QnUuid m_currentUserRoleId;
    ec2::ApiUserRoleDataList m_userRoles;
    QHash<QnUuid, QSet<QnUuid>> m_accessibleResources;
    QHash<QnUuid, UserRoleReplacement> m_replacements;
    QSet<QString> m_predefinedNames;
};
