#include "user_settings_widget.h"
#include "ui_user_settings_widget.h"

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/models/user_settings_model.h>
#include <ui/style/custom_style.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/email/email.h>

namespace
{
    const int kUserGroupIdRole = Qt::UserRole + 1;
    const int kPermissionsRole = Qt::UserRole + 2;
}

QnUserSettingsWidget::QnUserSettingsWidget(QnUserSettingsModel* model, QWidget* parent /*= 0*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserSettingsWidget()),
    m_model(model)
{
    ui->setupUi(this);

    setHelpTopic(ui->groupLabel, ui->groupComboBox, Qn::UserSettings_UserRoles_Help);
//    setHelpTopic(ui->accessRightsGroupbox, Qn::UserSettings_UserRoles_Help);
    setHelpTopic(ui->enabledButton, Qn::UserSettings_DisableUser_Help);

    connect(ui->loginEdit,              &QLineEdit::textChanged,                this, &QnUserSettingsWidget::updateLogin);
    connect(ui->loginEdit,              &QLineEdit::textChanged,                this, &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->passwordEdit, &QLineEdit::textChanged, this, &QnUserSettingsWidget::updatePassword);
    connect(ui->passwordEdit, &QLineEdit::textChanged, this, &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->confirmPasswordEdit, &QLineEdit::textChanged, this, &QnUserSettingsWidget::updatePassword);
    connect(ui->confirmPasswordEdit, &QLineEdit::textChanged, this, &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->emailEdit,              &QLineEdit::textChanged,                this, &QnUserSettingsWidget::updateEmail);
    connect(ui->emailEdit,              &QLineEdit::textChanged,                this, &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->enabledButton,          &QPushButton::clicked,                  this, &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->groupComboBox,          QnComboboxCurrentIndexChanged,          this, &QnUserSettingsWidget::hasChangesChanged);

    //setWarningStyle(ui->hintLabel);
}

QnUserSettingsWidget::~QnUserSettingsWidget()
{}

bool QnUserSettingsWidget::hasChanges() const
{
    if (m_model->mode() == QnUserSettingsModel::NewUser)
        return true;

    if (!validMode())
        return false;

    Qn::Permissions permissions = accessController()->permissions(m_model->user());

    if (permissions.testFlag(Qn::WriteNamePermission))
        if (m_model->user()->getName() != ui->loginEdit->text().trimmed())
            return true;

    if (permissions.testFlag(Qn::WritePasswordPermission) && !ui->passwordEdit->text().isEmpty()) //TODO: #GDM #access implement correct check
        return true;

    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
    {
        if (m_model->user()->isEnabled() != ui->enabledButton->isChecked())
            return true;

        QnUuid groupId = selectedUserGroup();
        if (m_model->user()->userGroup() != groupId)
            return true;

        if (groupId.isNull())
        {
            /* Check if we have selected a predefined internal group. */
            Qn::GlobalPermissions permissions = selectedPermissions();
            if (permissions != Qn::NoGlobalPermissions
                && permissions != qnResourceAccessManager->globalPermissions(m_model->user()))
                return true;
        }
    }

    if (permissions.testFlag(Qn::WriteEmailPermission))
        if (m_model->user()->getEmail() != ui->emailEdit->text())
            return true;

    return false;
}

void QnUserSettingsWidget::loadDataToUi()
{
    if (!validMode())
        return;

    updateAccessRightsPresets();
    updateControlsAccess();

    ui->loginEdit->setText(m_model->user()->getName());
    ui->emailEdit->setText(m_model->user()->getEmail());
    ui->passwordEdit->clear();
    ui->confirmPasswordEdit->clear();
    ui->enabledButton->setChecked(m_model->user()->isEnabled());

    /* If there is only one entry in permissions combobox, this check doesn't matter. */
    int customPermissionsIndex = ui->groupComboBox->count() - 1;
    NX_ASSERT(customPermissionsIndex == 0 ||
        ui->groupComboBox->itemData(customPermissionsIndex, kPermissionsRole).value<Qn::GlobalPermissions>() == Qn::NoGlobalPermissions);
    NX_ASSERT(customPermissionsIndex == 0 ||
        ui->groupComboBox->itemData(customPermissionsIndex, kUserGroupIdRole).value<QnUuid>().isNull());

    int permissionsIndex = customPermissionsIndex;
    Qn::GlobalPermissions permissions = qnResourceAccessManager->globalPermissions(m_model->user());

    if (!m_model->user()->userGroup().isNull())
    {
        permissionsIndex = ui->groupComboBox->findData(qVariantFromValue(m_model->user()->userGroup()), kUserGroupIdRole, Qt::MatchExactly);
    }
    else if (permissions != Qn::NoGlobalPermissions)
    {
        permissionsIndex = ui->groupComboBox->findData(qVariantFromValue(permissions), kPermissionsRole, Qt::MatchExactly);
    }


    if (permissionsIndex < 0)
        permissionsIndex = customPermissionsIndex;
    ui->groupComboBox->setCurrentIndex(permissionsIndex);
    ui->permissionsLabel->setText(m_model->groupDescription());

    updateLogin();
    updatePassword();
    updateEmail();
}

void QnUserSettingsWidget::applyChanges()
{
    if (!validMode())
        return;

    if (isReadOnly())
        return;

    Qn::Permissions permissions = accessController()->permissions(m_model->user());

    if (permissions.testFlag(Qn::WriteNamePermission))
        m_model->user()->setName(ui->loginEdit->text().trimmed());

    //empty text means 'no change'
    const QString newPassword = ui->passwordEdit->text().trimmed();
    if (permissions.testFlag(Qn::WritePasswordPermission) && !newPassword.isEmpty()) //TODO: #GDM #access implement correct check
    {
        m_model->user()->setPassword(newPassword);
        m_model->user()->generateHash();
        m_model->user()->setPassword(QString());
    }

    /* Here we must be sure settings widget goes before the permissions one. */
    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
    {
        QnUuid groupId = selectedUserGroup();
        if (!groupId.isNull())
            m_model->user()->setUserGroup(groupId);
        else
            m_model->user()->setRawPermissions(selectedPermissions());
    }

    if (permissions.testFlag(Qn::WriteEmailPermission))
        m_model->user()->setEmail(ui->emailEdit->text());

    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
        m_model->user()->setEnabled(ui->enabledButton->isChecked());
}

void QnUserSettingsWidget::updateLogin()
{
    QString proposedLogin = ui->loginEdit->text().trimmed().toLower();

    bool valid = true;
    QString hint;
    if (proposedLogin.isEmpty())
    {
        hint = tr("Login cannot be empty.");
        valid = false;
    }
    else
    {
        for (const QnUserResourcePtr& user : qnResPool->getResources<QnUserResource>())
        {
            if (user == m_model->user())
                continue;

            if (user->getName().toLower() != proposedLogin)
                continue;

            hint = tr("User with specified login already exists.");
            valid = false;
            break;
        }
    }

    QPalette palette = this->palette();
    if (!valid)
        setWarningStyle(&palette);

    ui->loginLabel->setPalette(palette);
    ui->loginEdit->setPalette(palette);

    /* Show warning message if we have renamed an existing user */
    if (m_model->mode() == QnUserSettingsModel::OtherSettings)
        updatePassword();
}

void QnUserSettingsWidget::updatePassword()
{
    bool valid = true;
    QString hint;

    /* Show warning message if we have renamed an existing user.. */
    if (m_model->mode() == QnUserSettingsModel::OtherSettings && ui->loginEdit->text().trimmed() != m_model->user()->getName())
    {
        /* ..and have not entered new password. */
        if (ui->passwordEdit->text().isEmpty())
        {
            hint = tr("User has been renamed. Password must be updated.");
            valid = false;
        }
    }

    /* Show warning if have not entered password for the new user. */
    if (m_model->mode() == QnUserSettingsModel::NewUser && ui->passwordEdit->text().isEmpty())
    {
        hint = tr("Password cannot be empty.");
        valid = false;
    }

}

void QnUserSettingsWidget::updateEmail()
{
    bool valid = true;
    QString hint;

    if (!ui->emailEdit->text().trimmed().isEmpty() && !QnEmailAddress::isValid(ui->emailEdit->text()))
    {
        hint = tr("Invalid email address.");
        valid = false;
    }

    QPalette palette = this->palette();
    if (!valid)
        setWarningStyle(&palette);

    ui->emailLabel->setPalette(palette);
    ui->emailEdit->setPalette(palette);
}

void QnUserSettingsWidget::updateControlsAccess()
{
    Qn::Permissions permissions = m_model->user()
        ? accessController()->permissions(m_model->user())
        : Qn::NoPermissions;

    ui->loginEdit->setReadOnly(!permissions.testFlag(Qn::WriteNamePermission));
    ui->passwordLabel->setVisible(permissions.testFlag(Qn::WritePasswordPermission));
    ui->passwordEdit->setVisible(permissions.testFlag(Qn::WritePasswordPermission));
    ui->confirmPasswordEdit->setVisible(permissions.testFlag(Qn::WritePasswordPermission));
    ui->confirmPasswordLabel->setVisible(permissions.testFlag(Qn::WritePasswordPermission));

    ui->emailEdit->setReadOnly(!permissions.testFlag(Qn::WriteEmailPermission));

    ui->enabledButton->setVisible(permissions.testFlag(Qn::WriteAccessRightsPermission));
}

void QnUserSettingsWidget::updateAccessRightsPresets()
{
    ui->groupComboBox->clear();

    if (!m_model->user())
        return;

    auto addBuiltInGroup = [this](const QString &name, Qn::GlobalPermissions permissions)
    {
        int index = ui->groupComboBox->count();
        ui->groupComboBox->insertItem(index, name);
        ui->groupComboBox->setItemData(index,   qVariantFromValue(QnUuid()),    kUserGroupIdRole);
        ui->groupComboBox->setItemData(index,   qVariantFromValue(permissions), kPermissionsRole);
    };

    auto addCustomGroup = [this](const ec2::ApiUserGroupData &group)
    {
        int index = ui->groupComboBox->count();
        ui->groupComboBox->insertItem(index, group.name);
        ui->groupComboBox->setItemData(index, qVariantFromValue(group.id),      kUserGroupIdRole);
    };

    Qn::GlobalPermissions permissions = qnResourceAccessManager->globalPermissions(m_model->user());

    // show only for view of owner
    if (permissions.testFlag(Qn::GlobalOwnerPermission))
        addBuiltInGroup(tr("Owner"), Qn::GlobalOwnerPermissionsSet);    /* Really we should never see this group. */

    // show for an admin or for anyone opened by owner
    if (permissions.testFlag(Qn::GlobalAdminPermission) || accessController()->hasGlobalPermission(Qn::GlobalOwnerPermission))
        addBuiltInGroup(tr("Administrator"), Qn::GlobalAdminPermissionsSet);

    addBuiltInGroup(tr("Advanced Viewer"),   Qn::GlobalAdvancedViewerPermissionSet);
    addBuiltInGroup(tr("Viewer"),            Qn::GlobalViewerPermissionSet);
    addBuiltInGroup(tr("Live Viewer"),       Qn::GlobalLiveViewerPermissionSet);

    bool isAdmin = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);
    for (const ec2::ApiUserGroupData& group : qnResourceAccessManager->userGroups())
    {
        if (isAdmin || group.id == m_model->user()->userGroup())
            addCustomGroup(group);
    }

    addBuiltInGroup(tr("New Group..."), Qn::NoGlobalPermissions);
    addBuiltInGroup(tr("Custom..."), Qn::NoGlobalPermissions);
}

Qn::GlobalPermissions QnUserSettingsWidget::selectedPermissions() const
{
    return ui->groupComboBox->itemData(ui->groupComboBox->currentIndex(), kPermissionsRole).value<Qn::GlobalPermissions>();
}

QnUuid QnUserSettingsWidget::selectedUserGroup() const
{
    return ui->groupComboBox->itemData(ui->groupComboBox->currentIndex(), kUserGroupIdRole).value<QnUuid>();
}

bool QnUserSettingsWidget::validMode() const
{
    return m_model->mode() == QnUserSettingsModel::OtherSettings
        || m_model->mode() == QnUserSettingsModel::NewUser;
}
