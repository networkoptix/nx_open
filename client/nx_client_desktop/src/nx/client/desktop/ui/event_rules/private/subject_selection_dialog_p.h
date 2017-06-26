#pragma once

#include <functional>

#include <QtGui/QValidator>

#include <common/common_module_aware.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/delegates/customizable_item_delegate.h>
#include <ui/models/user_roles_model.h>
#include <ui/models/resource/resource_list_model.h>
#include <ui/utils/validators.h>

#include <nx/client/desktop/ui/common/column_remap_proxy_model.h>
#include <nx/client/desktop/ui/common/natural_string_sort_proxy_model.h>
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

    void setUserValidator(Qn::UserValidator userValidator);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QSet<QnUuid> checkedUsers() const;

private:
    QValidator::State validate(const QnUuid& roleId) const;

private:
    Qn::UserValidator m_userValidator;
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

    static QnUserResourcePtr getUser(const QModelIndex& index);

private:
    void columnsChanged(int firstColumn, int lastColumn, const QVector<int> roles = {});
    void updateIndicators();

private:
    QnResourceListModel* const m_usersModel;
    RoleListModel* const m_rolesModel;
    Qn::UserValidator m_userValidator;
    bool m_customUsersOnly = true;
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

    virtual ItemState itemState(const QModelIndex& index) const override;
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
    using base_type::base_type; //< Forward constructors.

protected:
    virtual void initStyleOption(QStyleOptionViewItem* option,
        const QModelIndex& index) const override;
};

//-------------------------------------------------------------------------------------------------

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
