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
        replacementRoles(new QnUserRolesModel(this,
            true,  /* standardRoles */
            false, /* userRoles (will be filled later) */
            false  /* customRole */))
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

                disconnectUserSignals(user);

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
            [this](const QnResourcePtr& resource)
            {
                auto user = resource.staticCast<QnUserResource>();
                if (user->userGroup() == model->selectedGroup())
                    userAddedOrUpdated(user);
                else
                    userMaybeRemoved(user);
            });
    }

    void disconnectUserSignals(const QnUserResourcePtr& user)
    {
        user->disconnect(this);
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
        Q_Q(QnUserGroupSettingsWidget);
        QnUserGroupSettingsModel::RoleReplacement replacement(QnUuid(), Qn::GlobalLiveViewerPermissionSet);

        if (hasUsers())
        {
            QnMessageBox messageBox(QnMessageBox::Warning,
                Qn::Empty_Help, //TODO: #vkutin #GDM Change to correct topic
                q->ui->deleteGroupButton->text(),
                tr("Select a new role for users"),
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                q->window());

            replacementRoles->setUserRoles(model->groups());
            replacementRoles->removeUserRole(model->selectedGroup());

            messageBox.setInformativeText(tr("All users that had this role will be assigned the following role:"));
            messageBox.setComboBoxModel(replacementRoles);

            //TODO: #vkutin Find the best replacement instead of just choosing Live Viewer
            QnUuid bestReplacementId;
            Qn::GlobalPermissions bestReplacementPermissions = Qn::GlobalLiveViewerPermissionSet;

            QModelIndexList indices;
            if (bestReplacementId.isNull())
            {
                indices = replacementRoles->match(replacementRoles->index(0, 0),
                    Qn::GlobalPermissionsRole,
                    QVariant::fromValue(bestReplacementPermissions),
                    1, Qt::MatchExactly);
            }
            else
            {
                indices = replacementRoles->match(replacementRoles->index(0, 0),
                    Qn::UuidRole,
                    QVariant::fromValue(bestReplacementId),
                    1, Qt::MatchExactly);
            }

            messageBox.setCurrentComboBoxIndex(indices.empty() ? 0 : indices[0].row());

            if (messageBox.exec() != QDialogButtonBox::Ok)
                return;

            QModelIndex index = replacementRoles->index(messageBox.currentComboBoxIndex(), 0);
            NX_ASSERT(index.isValid());

            replacement = QnUserGroupSettingsModel::RoleReplacement(
                index.data(Qn::UuidRole).value<QnUuid>(),
                index.data(Qn::GlobalPermissionsRole).value<Qn::GlobalPermissions>());
        }

        model->removeGroup(model->selectedGroup(), replacement);

        /* If selection was changed to the replacement group then we
         *   must update its users list to include all new candidates: */
        if (replacement.group == model->selectedGroup())
            resetUsers();

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

public:
    QnUserGroupSettingsWidget* q_ptr;
    Q_DECLARE_PUBLIC(QnUserGroupSettingsWidget);

    QnUserGroupSettingsModel* model;
    QStandardItemModel* usersModel;
    QnUserRolesModel* replacementRoles;
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

    connect(ui->nameLineEdit, &QLineEdit::textChanged, this, &QnUserGroupSettingsWidget::applyChanges);
    connect(ui->deleteGroupButton, &QPushButton::clicked, d, &QnUserGroupSettingsWidgetPrivate::deleteCurrentGroup);
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

    QSignalBlocker blocker(ui->nameLineEdit);
    ui->nameLineEdit->setText(d->model->groupName());

    d->resetUsers();
}

void QnUserGroupSettingsWidget::applyChanges()
{
    Q_D(QnUserGroupSettingsWidget);

    d->model->setGroupName(ui->nameLineEdit->text());
    emit hasChangesChanged();
}
