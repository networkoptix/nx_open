#include "user_profile_widget.h"
#include "ui_user_profile_widget.h"

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
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

QnUserProfileWidget::QnUserProfileWidget(QWidget* parent /*= 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserProfileWidget()),
    m_mode(Mode::Invalid),
    m_user()
{
    ui->setupUi(this);

    setHelpTopic(ui->groupLabel, ui->groupComboBox, Qn::UserSettings_UserRoles_Help);
//    setHelpTopic(ui->accessRightsGroupbox, Qn::UserSettings_UserRoles_Help);
    setHelpTopic(ui->enabledButton, Qn::UserSettings_DisableUser_Help);

    connect(ui->loginEdit,              &QLineEdit::textChanged,                this, &QnUserProfileWidget::updateLogin);
    connect(ui->loginEdit,              &QLineEdit::textChanged,                this, &QnUserProfileWidget::hasChangesChanged);
    connect(ui->currentPasswordEdit,    &QLineEdit::textChanged,                this, &QnUserProfileWidget::updatePassword);
    connect(ui->currentPasswordEdit,    &QLineEdit::textChanged,                this, &QnUserProfileWidget::hasChangesChanged);
    connect(ui->passwordEdit,           &QLineEdit::textChanged,                this, &QnUserProfileWidget::updatePassword);
    connect(ui->passwordEdit,           &QLineEdit::textChanged,                this, &QnUserProfileWidget::hasChangesChanged);
    connect(ui->confirmPasswordEdit,    &QLineEdit::textChanged,                this, &QnUserProfileWidget::updatePassword);
    connect(ui->confirmPasswordEdit,    &QLineEdit::textChanged,                this, &QnUserProfileWidget::hasChangesChanged);
    connect(ui->emailEdit,              &QLineEdit::textChanged,                this, &QnUserProfileWidget::updateEmail);
    connect(ui->emailEdit,              &QLineEdit::textChanged,                this, &QnUserProfileWidget::hasChangesChanged);
    connect(ui->enabledButton,          &QPushButton::clicked,                  this, &QnUserProfileWidget::hasChangesChanged);
    connect(ui->groupComboBox,          QnComboboxCurrentIndexChanged,          this, &QnUserProfileWidget::hasChangesChanged);

    //setWarningStyle(ui->hintLabel);
}

QnUserProfileWidget::~QnUserProfileWidget()
{}

QnUserResourcePtr QnUserProfileWidget::user() const
{
    return m_user;
}

void QnUserProfileWidget::setUser(const QnUserResourcePtr &user)
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

bool QnUserProfileWidget::hasChanges() const
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
            /* Check if we have selected a predefined internal group. */
            Qn::GlobalPermissions permissions = selectedPermissions();
            if (permissions != Qn::NoGlobalPermissions
                && permissions != m_user->getPermissions())
                return true;
        }
    }

    if (permissions.testFlag(Qn::WriteEmailPermission))
        if (m_user->getEmail() != ui->emailEdit->text())
            return true;

    return false;
}

void QnUserProfileWidget::loadDataToUi()
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
}

void QnUserProfileWidget::applyChanges()
{
    if (!m_user)
        return;

    if (isReadOnly())
        return;

    Qn::Permissions permissions = accessController()->permissions(m_user);

    if (permissions.testFlag(Qn::WriteNamePermission))
        m_user->setName(ui->loginEdit->text().trimmed());

    //empty text means 'no change'
    const QString newPassword = ui->passwordEdit->text().trimmed();
    if (permissions.testFlag(Qn::WritePasswordPermission) && !newPassword.isEmpty()) //TODO: #GDM #access implement correct check
    {
        m_user->setPassword(newPassword);
        m_user->generateHash();
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

        m_user->setPassword(QString());
    }

    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
    {
        QnUuid groupId = selectedUserGroup();
        if (!groupId.isNull())
        {
            m_user->setUserGroup(groupId);
        }
        else
        {
            Qn::GlobalPermissions permissions = selectedPermissions();
            if (permissions != Qn::NoGlobalPermissions)
                m_user->setPermissions(permissions);
            /* Otherwise permissions must be loaded from custom tabs. */
        }

    }

    if (permissions.testFlag(Qn::WriteEmailPermission))
        m_user->setEmail(ui->emailEdit->text());

    if (permissions.testFlag(Qn::WriteAccessRightsPermission))
        m_user->setEnabled(ui->enabledButton->isChecked());
}

bool QnUserProfileWidget::isCustomAccessRights() const
{
    int index = ui->groupComboBox->currentIndex();
    return selectedPermissions() == Qn::NoGlobalPermissions
        && selectedUserGroup().isNull();
}

void QnUserProfileWidget::updateLogin()
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

void QnUserProfileWidget::updatePassword()
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

void QnUserProfileWidget::updateEmail()
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

void QnUserProfileWidget::updateControlsAccess()
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

void QnUserProfileWidget::updateAccessRightsPresets()
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

Qn::GlobalPermissions QnUserProfileWidget::selectedPermissions() const
{
    return ui->groupComboBox->itemData(ui->groupComboBox->currentIndex(), kPermissionsRole).value<Qn::GlobalPermissions>();
}

QnUuid QnUserProfileWidget::selectedUserGroup() const
{
    return ui->groupComboBox->itemData(ui->groupComboBox->currentIndex(), kUserGroupIdRole).value<QnUuid>();
}
