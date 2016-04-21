#include "user_profile_widget.h"
#include "ui_user_profile_widget.h"

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>

#include <ui/common/read_only.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/models/user_settings_model.h>
#include <ui/style/custom_style.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/email/email.h>

QnUserProfileWidget::QnUserProfileWidget(QnUserSettingsModel* model, QWidget* parent /*= 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserProfileWidget()),
    m_model(model)
{
    ui->setupUi(this);

    ::setReadOnly(ui->loginLineEdit, true);
    ::setReadOnly(ui->groupLineEdit, true);

    for (QLineEdit* edit : { ui->currentPasswordEdit, ui->passwordEdit, ui->confirmPasswordEdit })
        connect(edit, &QLineEdit::textChanged, this, &QnUserProfileWidget::updatePassword);

    connect(ui->emailEdit,              &QLineEdit::textChanged,                this, &QnUserProfileWidget::updateEmail);

    for (QLineEdit* edit : { ui->currentPasswordEdit, ui->passwordEdit, ui->confirmPasswordEdit, ui->emailEdit })
        connect(edit, &QLineEdit::textChanged, this, &QnUserProfileWidget::hasChangesChanged);

    //setWarningStyle(ui->hintLabel);
}

QnUserProfileWidget::~QnUserProfileWidget()
{}

bool QnUserProfileWidget::hasChanges() const
{
    if (!validMode())
        return false;

    Qn::Permissions permissions = accessController()->permissions(m_model->user());

    if (permissions.testFlag(Qn::WritePasswordPermission) && !ui->passwordEdit->text().isEmpty()) //TODO: #GDM #access implement correct check
        return true;

    if (permissions.testFlag(Qn::WriteEmailPermission))
        if (m_model->user()->getEmail() != ui->emailEdit->text())
            return true;

    return false;
}

void QnUserProfileWidget::loadDataToUi()
{
    if (!validMode())
        return;

    updateControlsAccess();

    ui->loginLineEdit->setText(m_model->user()->getName());
    ui->emailEdit->setText(m_model->user()->getEmail());
    ui->passwordEdit->clear();
    ui->confirmPasswordEdit->clear();
    ui->currentPasswordEdit->clear();
    ui->groupLineEdit->setText(getUserGroup());

    updatePassword();
    updateEmail();
}

void QnUserProfileWidget::applyChanges()
{
    if (!validMode())
        return;

    if (isReadOnly())
        return;

    Qn::Permissions permissions = accessController()->permissions(m_model->user());

    //empty text means 'no change'
    const QString newPassword = ui->passwordEdit->text().trimmed();
    if (permissions.testFlag(Qn::WritePasswordPermission) && !newPassword.isEmpty()) //TODO: #GDM #access implement correct check
    {
        m_model->user()->setPassword(newPassword);
        m_model->user()->generateHash();
        if (m_model->mode() == QnUserSettingsModel::OwnProfile)
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

        m_model->user()->setPassword(QString());
    }

    if (permissions.testFlag(Qn::WriteEmailPermission))
        m_model->user()->setEmail(ui->emailEdit->text());
}

void QnUserProfileWidget::updatePassword()
{
    bool valid = true;
    QString hint;

    /* Current password should be checked only if the user editing himself. */
    if (m_model->mode() == QnUserSettingsModel::OwnProfile && !ui->passwordEdit->text().isEmpty())
    {
        if (ui->currentPasswordEdit->text().isEmpty()) {
            hint = tr("To modify your password, please enter existing one.");
            valid = false;
        }
        else if (!m_model->user()->checkPassword(ui->currentPasswordEdit->text()))
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
    Qn::Permissions permissions = m_model->user()
        ? accessController()->permissions(m_model->user())
        : Qn::NoPermissions;

    /* User must confirm current password to change own password. */
    bool canChangePassword = permissions.testFlag(Qn::WritePasswordPermission);
    std::initializer_list<QWidget*> passwordWidgets = {
        ui->currentPasswordEdit,
        ui->currentPasswordLabel,
        ui->passwordEdit,
        ui->passwordLabel,
        ui->confirmPasswordEdit,
        ui->confirmPasswordLabel
    };

    for (auto widget : passwordWidgets)
        widget->setVisible(canChangePassword);

    ::setReadOnly(ui->emailEdit, !permissions.testFlag(Qn::WriteEmailPermission));
}

QString QnUserProfileWidget::getUserGroup() const
{
    Qn::GlobalPermissions permissions = qnResourceAccessManager->globalPermissions(m_model->user());

    if (permissions == Qn::GlobalOwnerPermissionsSet)
        return tr("Owner");

    if (permissions == Qn::GlobalAdminPermissionsSet)
        return tr("Administrator");

    for (const ec2::ApiUserGroupData& group : qnResourceAccessManager->userGroups())
        if (group.id == m_model->user()->userGroup())
            return group.name;

    if (permissions == Qn::GlobalAdvancedViewerPermissionSet)
        return tr("Advanced Viewer");

    if (permissions == Qn::GlobalViewerPermissionSet)
        return tr("Viewer");

    if (permissions == Qn::GlobalLiveViewerPermissionSet)
        return tr("Live Viewer");

    return tr("Custom Permissions");
}

bool QnUserProfileWidget::validMode() const
{
    return m_model->mode() == QnUserSettingsModel::OwnProfile
        || m_model->mode() == QnUserSettingsModel::OtherProfile;
}
