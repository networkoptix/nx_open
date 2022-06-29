// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_role_settings_widget.h"
#include "ui_user_role_settings_widget.h"

#include <QtGui/QStandardItemModel>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QRadioButton>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <ui/common/indents.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_properties/user_roles_settings_model.h>
#include <ui/models/user_roles_model.h>

using namespace nx::vms::client::desktop;

class QnUserRoleSettingsWidgetPrivate:
    public QObject,
    public nx::vms::client::core::CommonModuleAware
{
    using base_type = QObject;

    Q_DECLARE_TR_FUNCTIONS(QnUserRoleSettingsWidgetPrivate)
public:
    QnUserRoleSettingsWidgetPrivate(
        QnUserRoleSettingsWidget* parent, QnUserRolesSettingsModel* model)
        :
        base_type(parent),
        q_ptr(parent),
        model(model),
        usersModel(new QStandardItemModel(this)),
        replacementRolesModel(nullptr),
        replacementMessageBox(nullptr),
        deleteRadioButton(nullptr),
        changeRadioButton(nullptr),
        replacementComboBox(nullptr)
    {
        for (const auto& user: resourcePool()->getResources<QnUserResource>())
            connectUserSignals(user);

        connect(resourcePool(), &QnResourcePool::resourceAdded, this,
            [this](const QnResourcePtr& resource)
            {
                QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
                if (!user)
                    return;

                connectUserSignals(user);

                if (user->firstRoleId() == this->model->selectedUserRoleId())
                    userAddedOrUpdated(user);
            });

        connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
            [this](const QnResourcePtr& resource)
            {
                QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
                if (!user)
                    return;

                disconnect(user.get(), nullptr, this, nullptr);

                if (user->firstRoleId() == this->model->selectedUserRoleId())
                    userMaybeRemoved(user);
            });
    }

    void connectUserSignals(const QnUserResourcePtr& user)
    {
        connect(user.get(), &QnResource::nameChanged, this,
            [this](const QnResourcePtr& resource)
            {
                auto user = resource.staticCast<QnUserResource>();
                if (user->firstRoleId() == model->selectedUserRoleId())
                    userAddedOrUpdated(user);
            });

        connect(user.get(), &QnUserResource::userRolesChanged, this,
            [this](const QnUserResourcePtr& user)
            {
                if (user->firstRoleId() == model->selectedUserRoleId())
                    userAddedOrUpdated(user);
                else
                    userMaybeRemoved(user);
            });
    }

    void resetUsers()
    {
        QnUserResourceList users = model->users(true);
        usersModel->clear();
        for (const auto& user: users)
            addUser(user);

        correctUsersTable();
    }

    void addUser(const QnUserResourcePtr& user)
    {
        auto item = new QStandardItem(
            qnResIconCache->icon(QnResourceIconCache::User), user->getName());
        item->setData(QVariant::fromValue<QnResourcePtr>(user), Qn::ResourceRole);
        usersModel->appendRow(item);
    }

    bool hasUsers() const
    {
        int count = usersModel->rowCount();
        NX_ASSERT(count > 0);

        if (count > 1)
            return true;

        return usersModel->data(usersModel->index(0, 0), Qn::ResourceRole).isValid();
    }

    void deleteCurrentUserRole()
    {
        QnUserRolesSettingsModel::UserRoleReplacement replacement;
        if (hasUsers() && !queryRoleReplacement(replacement))
            return;

        model->removeUserRole(model->selectedUserRoleId(), replacement);

        /* If selection was changed to the replacement group then we
         *   must update its users list to include all new candidates: */
        if (replacement.userRoleId == model->selectedUserRoleId())
            resetUsers();

        Q_Q(QnUserRoleSettingsWidget);
        emit q->hasChangesChanged();
    }

    // User was assigned currently selected role, or its name was updated:
    void userAddedOrUpdated(const QnUserResourcePtr& user)
    {
        QModelIndex index = userIndex(user);
        if (index.isValid())
        {
            usersModel->setData(index, user->getName(), Qt::DisplayRole);
            return;
        }

        if (!hasUsers())
            usersModel->clear();

        addUser(user);
    }

    // User was possibly removed from currently selected role:
    void userMaybeRemoved(const QnUserResourcePtr& resource)
    {
        QModelIndex index = userIndex(resource);
        if (!index.isValid())
            return;

        usersModel->removeRow(index.row());
        correctUsersTable();
    }

    QModelIndex userIndex(const QnUserResourcePtr& user)
    {
        auto existingUsers = usersModel->match(usersModel->index(0, 0),
            Qn::ResourceRole,
            QVariant::fromValue<QnResourcePtr>(user),
            1, Qt::MatchExactly);

        return existingUsers.empty() ? QModelIndex() : existingUsers[0];
    }

    void correctUsersTable()
    {
        if (usersModel->rowCount() == 0)
            usersModel->appendRow(new QStandardItem(tr("No users have this role")));
    }

    bool queryRoleReplacement(QnUserRolesSettingsModel::UserRoleReplacement& replacement)
    {
        ensureReplacementHelpers();

        auto roles = model->userRoles();
        auto selectedRole = std::find_if(roles.begin(), roles.end(),
            [selectedId = model->selectedUserRoleId()](const nx::vms::api::UserRoleData& userRole)
            {
                return userRole.id == selectedId;
            });

        GlobalPermissions oldPermissions;
        if (selectedRole != roles.end())
        {
            oldPermissions = selectedRole->permissions;
            roles.erase(selectedRole);
        }

        replacementRolesModel->setUserRoles(roles);

        Qn::UserRole defaultReplacementRole =
            oldPermissions.testFlag(GlobalPermission::accessAllMedia)
                ? Qn::UserRole::liveViewer
                : Qn::UserRole::customPermissions;

        auto indices = replacementRolesModel->match(replacementRolesModel->index(0, 0),
            Qn::UserRoleRole,
            QVariant::fromValue(defaultReplacementRole),
            1, Qt::MatchExactly);

        replacementComboBox->setCurrentIndex(indices.empty() ? 0 : indices[0].row());

        deleteRadioButton->setChecked(true);

        if (replacementMessageBox->exec() != QDialogButtonBox::Ok)
            return false;

        if (deleteRadioButton->isChecked())
        {
            replacement = QnUserRolesSettingsModel::UserRoleReplacement();
            return true;
        }

        QModelIndex index = replacementRolesModel->index(replacementComboBox->currentIndex(), 0);
        NX_ASSERT(index.isValid());

        replacement = QnUserRolesSettingsModel::UserRoleReplacement(
            index.data(Qn::UuidRole).value<QnUuid>(),
            index.data(Qn::GlobalPermissionsRole).value<GlobalPermissions>());

        return true;
    }

private:
    void ensureReplacementHelpers()
    {
        if (replacementRolesModel)
        {
            NX_ASSERT(replacementMessageBox && replacementComboBox
                && deleteRadioButton && changeRadioButton);
            return;
        }

        NX_ASSERT(!replacementMessageBox && !replacementComboBox
            && !deleteRadioButton && !changeRadioButton);

        replacementRolesModel = new QnUserRolesModel(this, QnUserRolesModel::AssignableFlag
                                                         | QnUserRolesModel::CustomRoleFlag);
        replacementRolesModel->setCustomRoleStrings(
            tr("Custom with no permissions"),
            tr("Users will have no permissions unless changed later."));

        Q_Q(QnUserRoleSettingsWidget);

        const auto text = tr("Please select an action to perform on %n users with this role", "",
            usersModel->rowCount());

        replacementMessageBox = new QnMessageBox(QnMessageBoxIcon::Question, text,
            QString(), QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            QDialogButtonBox::Ok, q->window());

        auto customWidget = new QWidget(replacementMessageBox);

        deleteRadioButton = new QRadioButton(tr("Delete such users"), customWidget);
        deleteRadioButton->setChecked(true);

        changeRadioButton = new QRadioButton(tr("Assign a new role"), customWidget);

        replacementComboBox = new QComboBox(customWidget);
        replacementComboBox->setModel(replacementRolesModel);
        replacementComboBox->setEnabled(false);

        auto layout = new QVBoxLayout(customWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(deleteRadioButton);
        layout->addWidget(changeRadioButton);
        layout->addWidget(replacementComboBox);

        connect(changeRadioButton, &QRadioButton::toggled,
            replacementComboBox, &QComboBox::setEnabled);

        replacementMessageBox->addCustomWidget(customWidget);
    }

public:
    QnUserRoleSettingsWidget* q_ptr;
    Q_DECLARE_PUBLIC(QnUserRoleSettingsWidget)

    QnUserRolesSettingsModel* model;
    QStandardItemModel* usersModel;

private:
    QnUserRolesModel* replacementRolesModel;
    QnMessageBox* replacementMessageBox;
    QRadioButton* deleteRadioButton;
    QRadioButton* changeRadioButton;
    QComboBox* replacementComboBox;
};

QnUserRoleSettingsWidget::QnUserRoleSettingsWidget(
    QnUserRolesSettingsModel* model, QWidget* parent /*= 0*/)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserRoleSettingsWidget()),
    d_ptr(new QnUserRoleSettingsWidgetPrivate(this, model))
{
    Q_D(QnUserRoleSettingsWidget);

    ui->setupUi(this);
    ui->usersListTreeView->setModel(d->usersModel);

    ui->usersListTreeView->setProperty(nx::style::Properties::kSuppressHoverPropery, true);
    ui->usersListTreeView->setProperty(nx::style::Properties::kSideIndentation,
        QVariant::fromValue(QnIndents()));

    ui->nameInputField->setValidator(
        [this](const QString& text)
        {
            auto name = text.trimmed().toLower();
            if (name.isEmpty())
                return ValidationResult(tr("Role name cannot be empty."));

            auto model = d_ptr->model;
            for (const auto& userRole: model->userRoles())
            {
                if (userRole.id == model->selectedUserRoleId())
                    continue;

                if (userRole.name.trimmed().toLower() != name)
                    continue;

                return ValidationResult(tr("Role with same name already exists."));
            }

            auto predefined = userRolesManager()->predefinedRoles();
            predefined << Qn::UserRole::customPermissions << Qn::UserRole::customUserRole;
            for (auto role: predefined)
            {
                if (userRolesManager()->userRoleName(role).trimmed().toLower() != name)
                    continue;

                return ValidationResult(tr("Role with same name already exists."));
            }

            return ValidationResult::kValid;
        });

    connect(ui->nameInputField, &InputField::textChanged, this,
        [this]
        {
            ui->nameInputField->validate();
            applyChanges();
        });

    connect(ui->deleteGroupButton, &QPushButton::clicked, d,
        &QnUserRoleSettingsWidgetPrivate::deleteCurrentUserRole);
}

QnUserRoleSettingsWidget::~QnUserRoleSettingsWidget()
{
}

bool QnUserRoleSettingsWidget::hasChanges() const
{
    return false;
}

void QnUserRoleSettingsWidget::loadDataToUi()
{
    Q_D(QnUserRoleSettingsWidget);

    QSignalBlocker blocker(ui->nameInputField);
    ui->nameInputField->setText(d->model->userRoleName());

    d->resetUsers();
}

void QnUserRoleSettingsWidget::applyChanges()
{
    Q_D(QnUserRoleSettingsWidget);
    d->model->setUserRoleName(ui->nameInputField->text());
    emit hasChangesChanged();
}
