#include "user_group_settings_widget.h"
#include "ui_user_group_settings_widget.h"

#include <core/resource/user_resource.h>

#include <ui/common/indents.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_properties/user_groups_settings_model.h>
#include <ui/models/user_roles_model.h>
#include <ui/style/helper.h>
#include <ui/style/resource_icon_cache.h>

QnUserGroupSettingsWidget::QnUserGroupSettingsWidget(QnUserGroupSettingsModel* model, QWidget* parent /*= 0*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserGroupSettingsWidget()),
    m_model(model),
    m_usersModel(new QStandardItemModel(this)),
    m_replacementRoles(new QnUserRolesModel(this, true, false, false))
{
    ui->setupUi(this);
    ui->usersListTreeView->setModel(m_usersModel);

    ui->usersListTreeView->setProperty(style::Properties::kSuppressHoverPropery, true);
    ui->usersListTreeView->setProperty(style::Properties::kSideIndentation, QVariant::fromValue(QnIndents()));

    connect(ui->nameLineEdit, &QLineEdit::textChanged, this, &QnUserGroupSettingsWidget::applyChanges);

    connect(ui->deleteGroupButton, &QPushButton::clicked, this, [this]()
    {
        QnUserGroupSettingsModel::RoleReplacement replacement(QnUuid(), Qn::GlobalLiveViewerPermissionSet);

        if (!m_model->users().empty())
        {
            QnMessageBox messageBox(QnMessageBox::Warning,
                Qn::Empty_Help, //TODO: #vkutin #GDM Change to correct topic
                ui->deleteGroupButton->text(),
                tr("Select a new role for users"),
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                window());

            m_replacementRoles->setUserRoles(m_model->groups());
            m_replacementRoles->removeUserRole(m_model->selectedGroup());

            messageBox.setInformativeText(tr("All users that had this role will be assigned the following role:"));
            messageBox.setComboBoxModel(m_replacementRoles);

            //TODO: #vkutin Find the best replacement instead of just choosing Live Viewer
            QnUuid bestReplacementId;
            Qn::GlobalPermissions bestReplacementPermissions = Qn::GlobalLiveViewerPermissionSet;

            QModelIndexList indices;
            if (bestReplacementId.isNull())
            {
                indices = m_replacementRoles->match(m_replacementRoles->index(0, 0),
                    Qn::GlobalPermissionsRole,
                    QVariant::fromValue(bestReplacementPermissions),
                    1, Qt::MatchExactly);
            }
            else
            {
                indices = m_replacementRoles->match(m_replacementRoles->index(0, 0),
                    Qn::UuidRole,
                    QVariant::fromValue(bestReplacementId),
                    1, Qt::MatchExactly);
            }

            messageBox.setCurrentComboBoxIndex(indices.empty() ? 0 : indices[0].row());

            if (messageBox.exec() != QDialogButtonBox::Ok)
                return;

            QModelIndex index = m_replacementRoles->index(messageBox.currentComboBoxIndex(), 0);
            NX_ASSERT(index.isValid());

            replacement = QnUserGroupSettingsModel::RoleReplacement(
                index.data(Qn::UuidRole).value<QnUuid>(),
                index.data(Qn::GlobalPermissionsRole).value<Qn::GlobalPermissions>());
        }

        m_model->removeGroup(m_model->selectedGroup(), replacement);
        emit hasChangesChanged();
    });
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
    QSignalBlocker blocker(ui->nameLineEdit);
    ui->nameLineEdit->setText(m_model->groupName());

    QnUserResourceList users = m_model->users();

    m_usersModel->clear();
    for (const auto& user : users)
        m_usersModel->appendRow(new QStandardItem(qnResIconCache->icon(QnResourceIconCache::User), user->getName()));

    if (users.isEmpty())
        m_usersModel->appendRow(new QStandardItem(tr("No users have this role")));

}

void QnUserGroupSettingsWidget::applyChanges()
{
    m_model->setGroupName(ui->nameLineEdit->text());
    emit hasChangesChanged();
}
