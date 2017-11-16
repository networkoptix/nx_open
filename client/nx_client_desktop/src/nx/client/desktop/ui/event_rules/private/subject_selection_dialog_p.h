#pragma once

#include <functional>

#include <QtGui/QValidator>

#include <common/common_module_aware.h>
#include <core/resource_access/resource_access_subjects_cache.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/delegates/customizable_item_delegate.h>
#include <ui/models/user_roles_model.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/utils/validators.h>

#include <nx/client/desktop/common/models/column_remap_proxy_model.h>
#include <nx/client/desktop/common/models/natural_string_sort_proxy_model.h>
#include <nx/client/desktop/ui/event_rules/subject_selection_dialog.h>

class QnUuid;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

//-------------------------------------------------------------------------------------------------
// SubjectSelectionDialog::RoleListModel

class SubjectSelectionDialog::RoleListModel:
    public QnUserRolesModel,
    public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QnUserRolesModel;

public:
    explicit RoleListModel(QObject* parent);

    void setRoleValidator(Qn::RoleValidator roleValidator);
    void setUserValidator(Qn::UserValidator userValidator);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    // All users from explicitly checked roles, regardless of allUsers value.
    QSet<QnUuid> checkedUsers() const;

    QValidator::State validateRole(const QnUuid& roleId) const;
    QValidator::State validateUsers(const QList<QnResourceAccessSubject>& subjects) const;

    bool allUsers() const;
    void setAllUsers(bool value);

private:
    void emitDataChanged();

private:
    Qn::RoleValidator m_roleValidator;
    Qn::UserValidator m_userValidator; //< Not used if m_roleValidator is set.
    bool m_allUsers = false; //< All users are considered checked.
    mutable QHash<QnUuid, QValidator::State> m_validationStates;
};

//-------------------------------------------------------------------------------------------------
// SubjectSelectionDialog::UserListModel

class SubjectSelectionDialog::UserListModel:
    public NaturalStringSortProxyModel,
    public QnCommonModuleAware
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

    void setUserValidator(Qn::UserValidator userValidator);

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
    Qn::UserValidator m_userValidator;
    bool m_customUsersOnly = true; //< Non-custom users are filtered out.
    bool m_allUsers = false; //< All users are considered checked.
};

//------------------------------------------------------------------------------------------------
// SubjectSelectionDialog::RoleListDelegate

class SubjectSelectionDialog::RoleListDelegate:
    public QnResourceItemDelegate,
    public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = QnResourceItemDelegate;

public:
    explicit RoleListDelegate(QObject* parent);
    virtual ~RoleListDelegate() override;

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

    virtual void getDisplayInfo(const QModelIndex& index,
        QString& baseName, QString& extInfo) const override;
};

//-------------------------------------------------------------------------------------------------
// SubjectSelectionDialog::UserListDelegate

class SubjectSelectionDialog::UserListDelegate:
    public QnResourceItemDelegate
{
    Q_OBJECT
    using base_type = QnResourceItemDelegate;

public:
    explicit UserListDelegate(QObject* parent);

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option,
        const QModelIndex& index) const override;

    virtual ItemState itemState(const QModelIndex& index) const;
};

//-------------------------------------------------------------------------------------------------

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
