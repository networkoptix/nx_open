#include "user_profile_widget.h"
#include "ui_user_profile_widget.h"

#include <tuple>

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

    ui->emailInputField->setTitle(tr("Email"));
    ui->emailInputField->setValidator([](const QString& text)
    {
        if (!text.trimmed().isEmpty() && !QnEmailAddress::isValid(text))
            return QnInputField::ValidateResult(false, tr("Invalid email address."));

        return QnInputField::ValidateResult(true, QString());
    });

    ui->newPasswordInputField->setTitle(tr("New Password"));
    ui->newPasswordInputField->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    ui->newPasswordInputField->setValidator([](const QString& text)
    {
        return QnInputField::ValidateResult(true, QString());
    });

    ui->confirmPasswordInputField->setTitle(tr("Confirm Password"));
    ui->confirmPasswordInputField->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    ui->confirmPasswordInputField->setValidator([this](const QString& text)
    {
        if (ui->newPasswordInputField->text().isEmpty())
            return QnInputField::ValidateResult(true, QString());

        if (ui->newPasswordInputField->text() != text)
            return QnInputField::ValidateResult(false, tr("Passwords do not match."));

        return QnInputField::ValidateResult(true, QString());
    });

    ui->currentPasswordInputField->setTitle(tr("Current Password"));
    ui->currentPasswordInputField->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    ui->currentPasswordInputField->setValidator([this](const QString& text)
    {
        if (ui->newPasswordInputField->text().isEmpty())
            return QnInputField::ValidateResult(true, QString());

        if (text.isEmpty())
            return QnInputField::ValidateResult(false, tr("To modify your password, please enter existing one."));

        if (!m_model->user()->checkPassword(text))
            return QnInputField::ValidateResult(false, tr("Invalid current password."));

        return QnInputField::ValidateResult(true, QString());
    });

    for (QnInputField* field : { ui->currentPasswordInputField, ui->newPasswordInputField, ui->confirmPasswordInputField, ui->emailInputField })
        connect(field, &QnInputField::textChanged, this, &QnUserProfileWidget::hasChangesChanged);

    //setWarningStyle(ui->hintLabel);
}

QnUserProfileWidget::~QnUserProfileWidget()
{}

bool QnUserProfileWidget::hasChanges() const
{
    if (!validMode())
        return false;

    Qn::Permissions permissions = accessController()->permissions(m_model->user());

    if (permissions.testFlag(Qn::WritePasswordPermission) && !ui->newPasswordInputField->text().isEmpty()) //TODO: #GDM #access implement correct check
        return true;

    if (permissions.testFlag(Qn::WriteEmailPermission))
        if (m_model->user()->getEmail() != ui->emailInputField->text())
            return true;

    return false;
}

void QnUserProfileWidget::loadDataToUi()
{
    if (!validMode())
        return;

    updateControlsAccess();

    ui->loginLineEdit->setText(m_model->user()->getName());
    ui->groupLineEdit->setText(getUserGroup());

    ui->emailInputField->setText(m_model->user()->getEmail());
    ui->newPasswordInputField->clear();
    ui->confirmPasswordInputField->clear();
    ui->currentPasswordInputField->clear();
}

void QnUserProfileWidget::applyChanges()
{
    if (!validMode())
        return;

    if (isReadOnly())
        return;

    Qn::Permissions permissions = accessController()->permissions(m_model->user());

    //empty text means 'no change'
    const QString newPassword = ui->newPasswordInputField->text().trimmed();
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
        m_model->user()->setEmail(ui->emailInputField->text());
}

bool QnUserProfileWidget::canApplyChanges() const
{
    if (m_model->mode() != QnUserSettingsModel::OwnProfile)
        return true;

    for (QnInputField* field : { ui->currentPasswordInputField, ui->newPasswordInputField, ui->confirmPasswordInputField, ui->emailInputField })
        if (!field->isValid())
            return false;
    return true;
}

void QnUserProfileWidget::updateControlsAccess()
{
    Qn::Permissions permissions = m_model->user()
        ? accessController()->permissions(m_model->user())
        : Qn::NoPermissions;

    /* User must confirm current password to change own password. */
    bool canChangePassword = permissions.testFlag(Qn::WritePasswordPermission);
    for (QnInputField* field : { ui->currentPasswordInputField, ui->newPasswordInputField, ui->confirmPasswordInputField })
        field->setVisible(canChangePassword);

    ::setReadOnly(ui->emailInputField, !permissions.testFlag(Qn::WriteEmailPermission));
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
