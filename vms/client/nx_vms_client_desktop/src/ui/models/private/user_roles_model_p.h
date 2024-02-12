// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPersistentModelIndex>
#include <QtCore/QSet>

#include <ui/models/user_roles_model.h>
#include <ui/workbench/workbench_context_aware.h>

#include "../user_roles_model.h"

class QnUserRolesModel::Private: public QObject, public QnWorkbenchContextAware
{
    using base_type = QObject;
    QnUserRolesModel* const q;

public:
    explicit Private(QnUserRolesModel* q, QnUserRolesModel::DisplayRoleFlags flags);

    void setUserRoles(nx::vms::api::UserGroupDataList value);

    nx::vms::api::UserGroupData roleByRow(int row) const;
    int count() const;

    nx::Uuid id(int row) const;

    void setCustomRoleStrings(const QString& name, const QString& description);

private:
    bool updateUserRole(const nx::vms::api::UserGroupData& userRole);
    bool removeUserRole(const nx::vms::api::UserGroupData& userRole);
    bool removeUserRoleById(const nx::Uuid& roleId);

public:
    bool hasCheckBoxes = false;
    QSet<QPersistentModelIndex> checked;

private:
    nx::vms::api::UserGroupDataList m_userRoles;
    const bool m_customRoleEnabled;
    const bool m_onlyAssignable;

    QString m_customRoleName;
    QString m_customRoleDescription;
};
