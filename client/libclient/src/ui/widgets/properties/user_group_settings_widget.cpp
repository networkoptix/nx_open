#include "user_group_settings_widget.h"
#include "ui_user_group_settings_widget.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/indents.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_properties/user_groups_settings_model.h>
#include <ui/models/user_roles_model.h>
#include <ui/style/helper.h>
#include <ui/style/resource_icon_cache.h>


class QnUserGroupSettingsWidgetPrivate : public Connective<QObject>
{
    using base_type = Connective<QObject>;

public:
    QnUserGroupSettingsWidgetPrivate(QnUserGroupSettingsWidget* parent, QnUserGroupSettingsModel* model) :
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
        for (const auto& user : qnResPool->getResources<QnUserResource>())
            connectUserSignals(user);

        connect(qnResPool, &QnResourcePool::resourceAdded, this,
            [this](const QnResourcePtr& resource)
            {
                QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
                if (!user)
                    return;

                connectUserSignals(user);

                if (user->userGroup() == this->model->selectedGroup())
                    userAddedOrUpdated(user);
            });

        connect(qnResPool, &QnResourcePool::resourceRemoved, this,
            [this](const QnResourcePtr& resource)
            {
                QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
                if (!user)
                    return;

                disconnect(user, nullptr, this, nullptr);

                if (user->userGroup() == this->model->selectedGroup())
                    userMaybeRemoved(user);
            });
    }

    void connectUserSignals(const QnUserResourcePtr& user)
    {
        connect(user, &QnResource::nameChanged, this,
            [this](const QnResourcePtr& resource)
            {
                auto user = resource.staticCast<QnUserResource>();
                if (user->userGroup() == model->selectedGroup())
                    userAddedOrUpdated(user);
            });

        connect(user, &QnUserResource::userGroupChanged, this,
            [this](const QnUserResourcePtr& user)
            {
                if (user->userGroup() == model->selectedGroup())
                    userAddedOrUpdated(user);
                else
                    userMaybeRemoved(user);
            });
    }

    void resetUsers()
    {
        QnUserResourceList users = model->users(true);
        usersModel->clear();
        for (const auto& user : users)
            addUser(user);

        correctUsersTable();
    }

    void addUser(const QnUserResourcePtr& user)
    {
        auto item = new QStandardItem(qnResIconCache->icon(QnResourceIconCache::User), user->getName());
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

    void deleteCurrentGroup()
    {
        QnUserGroupSettingsModel::RoleReplacement replacement;
        if (hasUsers() && !queryRoleReplacement(replacement))
            return;

        model->removeGroup(model->selectedGroup(), replacement);

        /* If selection was changed to the replacement group then we
         *   must update its users list to include all new candidates: */
        if (replacement.group == model->selectedGroup())
            resetUsers();

        Q_Q(QnUserGroupSettingsWidget);
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

    bool queryRoleReplacement(QnUserGroupSettingsModel::RoleReplacement& replacement)
    {
        ensureReplacementHelpers();

        auto roles = model->groups();
        auto selectedRole = std::find_if(roles.begin(), roles.end(),
            [selectedId = model->selectedGroup()](const ec2::ApiUserGroupData& role)
            {
                return role.id == selectedId;
            });
        if (selectedRole != roles.end())
            roles.erase(selectedRole);

        replacementRolesModel->setUserRoles(roles);

        //TODO: #vkutin Find the best replacement instead of just choosing Live Viewer
        QnUuid bestReplacementId;
        Qn::GlobalPermissions bestReplacementPermissions = Qn::GlobalLiveViewerPermissionSet;

        QModelIndexList indices;
        if (bestReplacementId.isNull())
        {
            indices = replacementRolesModel->match(replacementRolesModel->index(0, 0),
                Qn::GlobalPermissionsRole,
                QVariant::fromValue(bestReplacementPermissions),
                1, Qt::MatchExactly);
        }
        else
        {
            indices = replacementRolesModel->match(replacementRolesModel->index(0, 0),
                Qn::UuidRole,
                QVariant::fromValue(bestReplacementId),
                1, Qt::MatchExactly);
        }

        replacementComboBox->setCurrentIndex(indices.empty() ? 0 : indices[0].row());

        deleteRadioButton->setChecked(true);

        if (replacementMessageBox->exec() != QDialogButtonBox::Ok)
            return false;

        if (deleteRadioButton->isChecked())
        {
            replacement = QnUserGroupSettingsModel::RoleReplacement::invalid();
            return true;
        }

        QModelIndex index = replacementRolesModel->index(replacementComboBox->currentIndex(), 0);
        NX_ASSERT(index.isValid());

        replacement = QnUserGroupSettingsModel::RoleReplacement(
            index.data(Qn::UuidRole).value<QnUuid>(),
            index.data(Qn::GlobalPermissionsRole).value<Qn::GlobalPermissions>());

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

        replacementRolesModel = new QnUserRolesModel(this, QnUserRolesModel::StandardRoleFlag);

        Q_Q(QnUserGroupSettingsWidget);

        replacementMessageBox = new QnMessageBox(QnMessageBox::Warning,
            Qn::Empty_Help, //TODO: #vkutin #GDM Change to correct topic
            q->ui->deleteGroupButton->text(),
            tr("Choose an action to do with users who had this role:"),
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            q->window());

        auto customWidget = new QWidget(replacementMessageBox);

        deleteRadioButton = new QRadioButton(tr("Delete"), customWidget);
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

        connect(changeRadioButton, &QRadioButton::toggled, replacementComboBox, &QComboBox::setEnabled);

        replacementMessageBox->addCustomWidget(customWidget);
    }

public:
    QnUserGroupSettingsWidget* q_ptr;
    Q_DECLARE_PUBLIC(QnUserGroupSettingsWidget);

    QnUserGroupSettingsModel* model;
    QStandardItemModel* usersModel;

private:
    QnUserRolesModel* replacementRolesModel;
    QnMessageBox* replacementMessageBox;
    QRadioButton* deleteRadioButton;
    QRadioButton* changeRadioButton;
    QComboBox* replacementComboBox;
};

QnUserGroupSettingsWidget::QnUserGroupSettingsWidget(QnUserGroupSettingsModel* model, QWidget* parent /*= 0*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserGroupSettingsWidget()),
    d_ptr(new QnUserGroupSettingsWidgetPrivate(this, model))
{
    Q_D(QnUserGroupSettingsWidget);

    ui->setupUi(this);
    ui->usersListTreeView->setModel(d->usersModel);

    ui->usersListTreeView->setProperty(style::Properties::kSuppressHoverPropery, true);
    ui->usersListTreeView->setProperty(style::Properties::kSideIndentation, QVariant::fromValue(QnIndents()));

    ui->nameInputField->setValidator(
        [this](const QString& text)
        {
            auto name = text.trimmed().toLower();
            if (name.isEmpty())
                return Qn::ValidationResult(tr("Role name cannot be empty."));

            auto model = d_ptr->model;
            for (const auto& role: model->groups())
            {
                if (role.id == model->selectedGroup())
                    continue;

                if (role.name.trimmed().toLower() != name)
                    continue;

                return Qn::ValidationResult(tr("Role with same name already exists."));
            }

            return Qn::kValidResult;
        });

    connect(ui->nameInputField, &QnInputField::textChanged, this,
        &QnUserGroupSettingsWidget::applyChanges);
    connect(ui->deleteGroupButton, &QPushButton::clicked, d,
        &QnUserGroupSettingsWidgetPrivate::deleteCurrentGroup);
}

QnUserGroupSettingsWidget::~QnUserGroupSettingsWidget()
{
}

bool QnUserGroupSettingsWidget::hasChanges() const
{
    return false;
}

void QnUserGroupSettingsWidget::loadDataToUi()
{
    Q_D(QnUserGroupSettingsWidget);

    QSignalBlocker blocker(ui->nameInputField);
    ui->nameInputField->setText(d->model->groupName());

    d->resetUsers();
}

void QnUserGroupSettingsWidget::applyChanges()
{
    ui->nameInputField->validate();
    if (!ui->nameInputField->isValid())
        return;

    Q_D(QnUserGroupSettingsWidget);

    d->model->setGroupName(ui->nameInputField->text());
    emit hasChangesChanged();
}
