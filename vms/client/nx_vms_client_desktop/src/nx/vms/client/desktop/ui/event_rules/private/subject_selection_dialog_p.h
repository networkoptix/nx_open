// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtGui/QValidator>

#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/desktop/common/delegates/customizable_item_delegate.h>
#include <nx/vms/client/desktop/common/models/column_remap_proxy_model.h>
#include <nx/vms/client/desktop/common/models/natural_string_sort_proxy_model.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/models/user_roles_model.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace subject_selection_dialog_private {

//-------------------------------------------------------------------------------------------------
// subject_selection_dialog_private::RoleListModel

class RoleListModel:
    public QnUserRolesModel,
    public core::CommonModuleAware
{
    Q_OBJECT
    using base_type = QnUserRolesModel;

public:
    explicit RoleListModel(QObject* parent);

    void setRoleValidator(RoleValidator roleValidator);
    void setUserValidator(UserValidator userValidator);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // All users from explicitly checked roles, regardless of allUsers value.
    QSet<QnUuid> checkedUsers() const;

    QValidator::State validateRole(const QnUuid& roleId) const;
    QValidator::State validateUsers(std::vector<QnResourceAccessSubject> subjects) const;

    bool allUsers() const;
    void setAllUsers(bool value);

private:
    void emitDataChanged();

private:
    RoleValidator m_roleValidator;
    UserValidator m_userValidator; //< Not used if m_roleValidator is set.
    bool m_allUsers = false; //< All users are considered checked.
    mutable QHash<QnUuid, QValidator::State> m_validationStates;
};

//-------------------------------------------------------------------------------------------------
// subject_selection_dialog_private::UserListModel

class UserListModel:
    public NaturalStringSortProxyModel,
    public core::CommonModuleAware
{
    Q_OBJECT
    using base_type = NaturalStringSortProxyModel;

public:
    enum Columns
    {
        NameColumn,
        IndicatorColumn,
        CheckColumn,
        ColumnCount
    };

    explicit UserListModel(RoleListModel* rolesModel, QObject* parent);

    // Explicitly checked users, regardless of allUsers value.
    QSet<QnUuid> checkedUsers() const;

    void setCheckedUsers(const QSet<QnUuid>& ids);

    void setUserValidator(UserValidator userValidator);

    bool customUsersOnly() const;
    void setCustomUsersOnly(bool value);

    bool systemHasCustomUsers() const;

    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

    bool isValid(const QModelIndex& index) const;
    bool isChecked(const QModelIndex& index) const;
    bool isIndirectlyChecked(const QModelIndex& index) const;

    bool allUsers() const;
    void setAllUsers(bool value);

    static QnUserResourcePtr getUser(const QModelIndex& index);

private:
    void columnsChanged(int firstColumn, int lastColumn, const QVector<int> roles = {});
    void updateIndicators();

private:
    QnResourceListModel* const m_usersModel;
    RoleListModel* const m_rolesModel;
    UserValidator m_userValidator;
    bool m_customUsersOnly = true; //< Non-custom users are filtered out.
    bool m_allUsers = false; //< All users are considered checked.
};

//------------------------------------------------------------------------------------------------
// subject_selection_dialog_private::GroupListDelegate

class GroupListDelegate:
    public QnResourceItemDelegate,
    public core::CommonModuleAware
{
    Q_OBJECT
    using base_type = QnResourceItemDelegate;

public:
    explicit GroupListDelegate(QObject* parent);
    virtual ~GroupListDelegate() override;

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

    virtual void getDisplayInfo(const QModelIndex& index,
        QString& baseName, QString& extInfo) const override;
};

//-------------------------------------------------------------------------------------------------
// subject_selection_dialog_private::UserListDelegate

class UserListDelegate:
    public QnResourceItemDelegate
{
    Q_OBJECT
    using base_type = QnResourceItemDelegate;

public:
    explicit UserListDelegate(QObject* parent);

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

    virtual ItemState itemState(const QModelIndex& index) const override;
};

//-------------------------------------------------------------------------------------------------

} // namespace subject_selection_dialog_private
} // namespace ui
} // namespace nx::vms::client::desktop
