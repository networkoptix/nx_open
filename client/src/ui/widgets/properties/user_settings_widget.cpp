#include "user_settings_widget.h"
#include "ui_user_settings_widget.h"

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/user_resource.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
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

QnUserSettingsWidget::QnUserSettingsWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserSettingsWidget()),
    m_mode(Mode::Invalid),
    m_user()
{
    ui->setupUi(this);

    setHelpTopic(ui->groupLabel, ui->groupComboBox, Qn::UserSettings_UserRoles_Help);
//    setHelpTopic(ui->accessRightsGroupbox, Qn::UserSettings_UserRoles_Help);
    setHelpTopic(ui->enabledButton, Qn::UserSettings_DisableUser_Help);

    connect(ui->loginEdit,              &QLineEdit::textChanged,                this, &QnUserSettingsWidget::updateLogin);
    connect(ui->loginEdit,              &QLineEdit::textChanged,                this, &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->currentPasswordEdit,    &QLineEdit::textChanged,                this, &QnUserSettingsWidget::updatePassword);
    connect(ui->currentPasswordEdit,    &QLineEdit::textChanged,                this, &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->passwordEdit,           &QLineEdit::textChanged,                this, &QnUserSettingsWidget::updatePassword);
    connect(ui->passwordEdit,           &QLineEdit::textChanged,                this, &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->confirmPasswordEdit,    &QLineEdit::textChanged,                this, &QnUserSettingsWidget::updatePassword);
    connect(ui->confirmPasswordEdit,    &QLineEdit::textChanged,                this, &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->emailEdit,              &QLineEdit::textChanged,                this, &QnUserSettingsWidget::updateEmail);
    connect(ui->emailEdit,              &QLineEdit::textChanged,                this, &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->enabledButton,          &QPushButton::clicked,                  this, &QnUserSettingsWidget::hasChangesChanged);
    connect(ui->groupComboBox,          QnComboboxCurrentIndexChanged,          this, &QnUserSettingsWidget::updateAccessRights);
    connect(ui->groupComboBox,          QnComboboxCurrentIndexChanged,          this, &QnUserSettingsWidget::hasChangesChanged);

    //setWarningStyle(ui->hintLabel);
}

QnUserSettingsWidget::~QnUserSettingsWidget()
{}

QnUserResourcePtr QnUserSettingsWidget::user() const
{
    return m_user;
}

void QnUserSettingsWidget::setUser(const QnUserResourcePtr &user)
{
    if (m_user == user)
        return;

    m_user = user;

    m_mode = !m_user
        ? Mode::Invalid
        : m_user->flags().testFlag(Qn::local)
        ? Mode::NewUser
        : m_user == context()->user()
        ? Mode::OwnUser
        : Mode::OtherUser;
}

bool QnUserSettingsWidget::hasChanges() const
{
    if (!m_user)
        return false;

    if (m_mode == Mode::NewUser)
        return true;

    Qn::Permissions permissions = accessController()->permissions(m_user);

    if (permissions.testFlag(Qn::WriteNamePermission))
        if (m_user->getName() != ui->loginEdit->text().trimmed())
            return true;

    if (permissions.testFlag(Qn::WritePasswordPermission) && !ui->passwordEdit->text().isEmpty()) //TODO: #GDM #access implement correct check
        return true;

    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
    {
        if (m_user->isEnabled() != ui->enabledButton->isChecked())
            return true;

        QnUuid groupId = selectedUserGroup();
        if (!groupId.isNull())
        {
            if (m_user->userGroup() != groupId)
                return true;
        }
        else
        {
            Qn::GlobalPermissions permissions = selectedPermissions();
            if (permissions != m_user->getPermissions())
                return true;
        }


    }

    if (permissions.testFlag(Qn::WriteEmailPermission))
        if (m_user->getEmail() != ui->emailEdit->text())
            return true;

    return false;
}

void QnUserSettingsWidget::loadDataToUi()
{
    updateAccessRightsPresets();
    updateControlsAccess();

    if (!m_user)
        return;

    ui->loginEdit->setText(m_user->getName());
    ui->emailEdit->setText(m_user->getEmail());
    ui->passwordEdit->clear();
    ui->confirmPasswordEdit->clear();
    ui->currentPasswordEdit->clear();
    ui->enabledButton->setChecked(m_user->isEnabled());

    /* If there is only one entry in permissions combobox, this check doesn't matter. */
    int customPermissionsIndex = ui->groupComboBox->count() - 1;
    NX_ASSERT(customPermissionsIndex == 0 ||
        ui->groupComboBox->itemData(customPermissionsIndex, kPermissionsRole).value<Qn::GlobalPermissions>() == Qn::NoGlobalPermissions);
    NX_ASSERT(customPermissionsIndex == 0 ||
        ui->groupComboBox->itemData(customPermissionsIndex, kUserGroupIdRole).value<QnUuid>().isNull());

    int permissionsIndex = customPermissionsIndex;
    Qn::GlobalPermissions permissions = qnResourceAccessManager->globalPermissions(m_user);

    if (!m_user->userGroup().isNull())
    {
        permissionsIndex = ui->groupComboBox->findData(qVariantFromValue(m_user->userGroup()), kUserGroupIdRole, Qt::MatchExactly);
    }
    else if (permissions != Qn::NoGlobalPermissions)
    {
        permissionsIndex = ui->groupComboBox->findData(qVariantFromValue(permissions), kPermissionsRole, Qt::MatchExactly);
    }


    if (permissionsIndex < 0)
        permissionsIndex = customPermissionsIndex;
    ui->groupComboBox->setCurrentIndex(permissionsIndex);


    updateLogin();
    updatePassword();
    updateEmail();
    updateAccessRights();
}

void QnUserSettingsWidget::applyChanges()
{
    if (!m_user)
        return;

    if (isReadOnly())
        return;

    //TODO: #GDM #access SafeMode what to rollback if current password changes cannot be saved?
    qnResourcesChangesManager->saveUser(m_user, [this](const QnUserResourcePtr &user)
    {
        Qn::Permissions permissions = accessController()->permissions(user);

        if (permissions.testFlag(Qn::WriteNamePermission))
            user->setName(ui->loginEdit->text().trimmed());

        //empty text means 'no change'
        const QString newPassword = ui->passwordEdit->text().trimmed();
        if (permissions.testFlag(Qn::WritePasswordPermission) && !newPassword.isEmpty()) //TODO: #GDM #access implement correct check
        {
            user->setPassword(newPassword);
            user->generateHash();
            if (m_mode == Mode::OwnUser)
            {
                /* Password was changed. Change it in global settings and hope for the best. */
                QUrl url = QnAppServerConnectionFactory::url();
                url.setPassword(newPassword);
                //// TODO #elric: This is a totally evil hack. Store password hash/salt in user.
                //context()->instance<QnWorkbenchUserWatcher>()->setUserPassword(newPassword);
                QnAppServerConnectionFactory::setUrl(url);

                //QnConnectionDataList savedConnections = qnSettings->customConnections();
                //if (!savedConnections.isEmpty()
                //    && !savedConnections.first().url.password().isEmpty()
                //    && qnUrlEqual(savedConnections.first().url, url))
                //{
                //    QnConnectionData current = savedConnections.takeFirst();
                //    current.url = url;
                //    savedConnections.prepend(current);
                //    qnSettings->setCustomConnections(savedConnections);
                //}

                //QnConnectionData lastUsed = qnSettings->lastUsedConnection();
                //if (!lastUsed.url.password().isEmpty() && qnUrlEqual(lastUsed.url, url)) {
                //    lastUsed.url = url;
                //    qnSettings->setLastUsedConnection(lastUsed);
                //}

            }

            user->setPassword(QString());
        }

        if (permissions.testFlag(Qn::WriteAccessRightsPermission))
        {
            QnUuid groupId = selectedUserGroup();
            if (!groupId.isNull())
            {
                user->setUserGroup(groupId);
            }
            else
            {
                Qn::GlobalPermissions permissions = selectedPermissions();
                if (permissions != Qn::NoGlobalPermissions)
                    user->setPermissions(permissions);
                /* Otherwise permissions must be loaded from custom tabs. */
            }

        }

        if (permissions.testFlag(Qn::WriteEmailPermission))
            user->setEmail(ui->emailEdit->text());

        if (permissions.testFlag(Qn::WriteAccessRightsPermission))
            user->setEnabled(ui->enabledButton->isChecked());


    });

}

bool QnUserSettingsWidget::isCustomAccessRights() const
{
    int index = ui->groupComboBox->currentIndex();
    return selectedPermissions() == Qn::NoGlobalPermissions
        && selectedUserGroup().isNull();
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
            if (user == m_user)
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
    if (m_mode == Mode::OtherUser)
        updatePassword();
}

void QnUserSettingsWidget::updatePassword()
{
    bool valid = true;
    QString hint;

    /* Current password should be checked only if the user editing himself. */
    if (m_mode == Mode::OwnUser && !ui->passwordEdit->text().isEmpty())
    {
        if (ui->currentPasswordEdit->text().isEmpty()) {
            hint = tr("To modify your password, please enter existing one.");
            valid = false;
        }
        else if (!m_user->checkPassword(ui->currentPasswordEdit->text()))
        {
            hint = tr("Invalid current password.");
            valid = false;
        }
        else if (ui->passwordEdit->text() != ui->confirmPasswordEdit->text())
        {
            hint = tr("Passwords do not match.");
            valid = false;
        }
    }

    /* Show warning message if we have renamed an existing user.. */
    if (m_mode == Mode::OtherUser && ui->loginEdit->text().trimmed() != m_user->getName())
    {
        /* ..and have not entered new password. */
        if (ui->passwordEdit->text().isEmpty())
        {
            hint = tr("User has been renamed. Password must be updated.");
            valid = false;
        }
    }

    /* Show warning if have not entered password for the new user. */
    if (m_mode == Mode::NewUser && ui->passwordEdit->text().isEmpty())
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

void QnUserSettingsWidget::updateAccessRights()
{
    if (isCustomAccessRights())
    {

    }
    else
    {


    }
}

void QnUserSettingsWidget::updateControlsAccess()
{
    Qn::Permissions permissions = m_user
        ? accessController()->permissions(m_user)
        : Qn::NoPermissions;

    ui->loginEdit->setReadOnly(!permissions.testFlag(Qn::WriteNamePermission));

    /* User must confirm current password to change own password. */
    bool requirePasswordConfirmation = permissions.testFlag(Qn::WritePasswordPermission)
        && !permissions.testFlag(Qn::WriteAccessRightsPermission);
    ui->currentPasswordEdit->setVisible(requirePasswordConfirmation);
    ui->currentPasswordLabel->setVisible(requirePasswordConfirmation);

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

    if (!m_user)
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

    Qn::GlobalPermissions permissions = qnResourceAccessManager->globalPermissions(m_user);

    // show only for view of owner
    if (permissions.testFlag(Qn::GlobalOwnerPermission))
        addBuiltInGroup(tr("Owner"), Qn::GlobalOwnerPermissionsSet);

    // show for an admin or for anyone opened by owner
    if (permissions.testFlag(Qn::GlobalAdminPermission) || accessController()->hasGlobalPermission(Qn::GlobalOwnerPermission))
        addBuiltInGroup(tr("Administrator"), Qn::GlobalAdminPermissionsSet);

    addBuiltInGroup(tr("Advanced Viewer"),   Qn::GlobalAdvancedViewerPermissionSet);
    addBuiltInGroup(tr("Viewer"),            Qn::GlobalViewerPermissionSet);
    addBuiltInGroup(tr("Live Viewer"),       Qn::GlobalLiveViewerPermissionSet);

    bool isAdmin = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);
    for (const ec2::ApiUserGroupData& group : qnResourceAccessManager->userGroups())
    {
        if (isAdmin || group.id == m_user->userGroup())
            addCustomGroup(group);
    }

    addBuiltInGroup(tr("New Group..."), Qn::NoGlobalPermissions);
    addBuiltInGroup(tr("Custom..."), Qn::NoGlobalPermissions);
}

void QnUserSettingsWidget::createAccessRightsAdvanced()
{
  /*  if (!m_user)
        return;

    Qn::GlobalPermissions permissions = qnResourceAccessManager->globalPermissions(m_user);
    QWidget* previous = ui->advancedButton;

    if (permissions.testFlag(Qn::GlobalOwnerPermission))
        previous = createAccessRightCheckBox(tr("Owner"), Qn::GlobalOwnerPermission, previous);

    if (permissions.testFlag(Qn::GlobalAdminPermission) || accessController()->hasGlobalPermission(Qn::GlobalOwnerPermission))
        previous = createAccessRightCheckBox(tr("Administrator"),
            Qn::GlobalAdminPermission,
            previous);

    previous = createAccessRightCheckBox(QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Can adjust devices settings"),
        tr("Can adjust cameras settings")
        ), Qn::GlobalEditCamerasPermission, previous);
    previous = createAccessRightCheckBox(tr("Can use PTZ controls"), Qn::GlobalPtzControlPermission, previous);
    previous = createAccessRightCheckBox(tr("Can view video archives"), Qn::GlobalViewArchivePermission, previous);
    previous = createAccessRightCheckBox(tr("Can export video"), Qn::GlobalExportPermission, previous);
    createAccessRightCheckBox(tr("Can edit Video Walls"), Qn::GlobalEditVideoWallPermission, previous);

    updateDependantPermissions();*/
}

QCheckBox *QnUserSettingsWidget::createAccessRightCheckBox(QString text, int right, QWidget *previous)
{
    return nullptr;
  /*  QCheckBox *checkBox = new QCheckBox(text, this);
    ui->accessRightsGroupbox->layout()->addWidget(checkBox);
    m_advancedRights.insert(right, checkBox);
    setTabOrder(previous, checkBox);

    if (isReadOnly(ui->accessRightsGroupbox))
        setReadOnly(checkBox, true);
    else
        connect(checkBox, &QCheckBox::clicked, this, &QnUserSettingsWidget::at_customCheckBox_clicked);
    return checkBox;*/
}

void QnUserSettingsWidget::selectAccessRightsPreset(int rights)
{
  /*  bool custom = true;
    for (int i = 0; i < ui->groupComboBox->count(); i++)
    {
        if (ui->groupComboBox->itemData(i).toULongLong() == rights)
        {
            ui->groupComboBox->setCurrentIndex(i);
            custom = false;
            break;
        }
    }

    if (custom)
    {
        ui->advancedButton->setChecked(true);
        ui->groupComboBox->setCurrentIndex(ui->groupComboBox->count() - 1);
    }*/
}

void QnUserSettingsWidget::fillAccessRightsAdvanced(int rights)
{
    //if (m_inUpdateDependensies)
    //    return; //just in case

    //m_inUpdateDependensies = true;

    //for (auto pos = m_advancedRights.begin(); pos != m_advancedRights.end(); pos++)
    //    if (pos.value())
    //        pos.value()->setChecked((pos.key() & rights) == pos.key());
    //m_inUpdateDependensies = false;

    //updateDependantPermissions(); // TODO: #GDM #Common rename to something more sane, connect properly
}

Qn::GlobalPermissions QnUserSettingsWidget::selectedPermissions() const
{
    return ui->groupComboBox->itemData(ui->groupComboBox->currentIndex(), kPermissionsRole).value<Qn::GlobalPermissions>();
}

QnUuid QnUserSettingsWidget::selectedUserGroup() const
{
    return ui->groupComboBox->itemData(ui->groupComboBox->currentIndex(), kUserGroupIdRole).value<QnUuid>();
}

// Utility functions
//
//bool QnUserSettingsWidget::isCheckboxChecked(int right) {
//    return m_advancedRights[right] ? m_advancedRights[right]->isChecked() : false;
//}
//
//void QnUserSettingsWidget::setCheckboxChecked(int right, bool checked) {
//    if (QCheckBox *targetCheckBox = m_advancedRights[right]) {
//        targetCheckBox->setChecked(checked);
//    }
//}
//
//void QnUserSettingsWidget::setCheckboxEnabled(int right, bool enabled) {
//    if (QCheckBox *targetCheckBox = m_advancedRights[right]) {
//        targetCheckBox->setEnabled(enabled);
//    }
//}
