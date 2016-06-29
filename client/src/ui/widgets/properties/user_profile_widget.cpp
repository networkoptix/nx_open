#include "user_profile_widget.h"
#include "ui_user_profile_widget.h"

#include <api/app_server_connection.h>

#include <client/client_settings.h>
#include <client_core/client_core_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>
#include <core/resource/user_resource.h>

#include <ui/common/read_only.h>
#include <ui/common/aligner.h>
#include <ui/dialogs/resource_properties/change_user_password_dialog.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/models/resource_properties/user_settings_model.h>
#include <ui/style/custom_style.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/watchers/workbench_user_watcher.h>

#include <utils/email/email.h>
#include <utils/common/url.h>

QnUserProfileWidget::QnUserProfileWidget(QnUserSettingsModel* model, QWidget* parent /*= 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserProfileWidget()),
    m_model(model),
    m_newPassword()
{
    ui->setupUi(this);

    ::setReadOnly(ui->loginInputField, true);
    ::setReadOnly(ui->groupInputField, true);

    ui->loginInputField->setTitle(tr("Login"));
    ui->nameInputField->setTitle(tr("Name"));
    ui->groupInputField->setTitle(tr("Role"));

    ui->emailInputField->setTitle(tr("Email"));
    ui->emailInputField->setValidator(Qn::defaultEmailValidator());

    connect(ui->emailInputField, &QnInputField::textChanged, this, &QnUserProfileWidget::hasChangesChanged);

    connect(ui->changePasswordButton, &QPushButton::clicked, this, [this]()
    {
        if (!validMode())
            return;

        Qn::Permissions permissions = accessController()->permissions(m_model->user());
        if (!permissions.testFlag(Qn::WritePasswordPermission))
            return;

        QScopedPointer<QnChangeUserPasswordDialog> dialog(new QnChangeUserPasswordDialog());
        dialog->initializeContext(this);
        dialog->setWindowModality(Qt::ApplicationModal);
        if (dialog->exec() != QDialog::Accepted)
            return;
        m_newPassword = dialog->newPassword();
    });

    QnAligner* aligner = new QnAligner(this);
    aligner->registerTypeAccessor<QnInputField>(QnInputField::createLabelWidthAccessor());

    aligner->addWidgets({
        ui->loginInputField,
        ui->nameInputField,
        ui->groupInputField,
        ui->changePasswordSpacerLabel,
        ui->permissionsSpacerLabel,
        ui->emailInputField
    });
}

QnUserProfileWidget::~QnUserProfileWidget()
{
}

bool QnUserProfileWidget::hasChanges() const
{
    if (!validMode())
        return false;

    Qn::Permissions permissions = accessController()->permissions(m_model->user());

    if (permissions.testFlag(Qn::WritePasswordPermission) && !m_newPassword.isEmpty())
        return true;

    if (permissions.testFlag(Qn::WriteEmailPermission))
        if (m_model->user()->getEmail() != ui->emailInputField->text())
            return true;

    if (permissions.testFlag(Qn::WriteFullNamePermission))
        if (m_model->user()->fullName() != ui->nameInputField->text().trimmed())
            return true;

    return false;
}

void QnUserProfileWidget::loadDataToUi()
{
    if (!validMode())
        return;

    updateControlsAccess();

    ui->loginInputField->setText(m_model->user()->getName());
    ui->nameInputField->setText(m_model->user()->fullName());
    ui->groupInputField->setText(accessController()->userRoleName(m_model->user()));
    ui->emailInputField->setText(m_model->user()->getEmail());
    m_newPassword.clear();
}

void QnUserProfileWidget::updatePermissionsLabel(const QString& text)
{
    ui->permissionsLabel->setText(text);
}

void QnUserProfileWidget::applyChanges()
{
    if (!validMode())
        return;

    if (isReadOnly())
        return;

    Qn::Permissions permissions = accessController()->permissions(m_model->user());

    //empty text means 'no change'
    if (permissions.testFlag(Qn::WritePasswordPermission) && !m_newPassword.isEmpty())
    {
        m_model->user()->setPassword(m_newPassword);
        m_model->user()->generateHash();
        if (m_model->mode() == QnUserSettingsModel::OwnProfile)
        {
            /* Password was changed. Change it in global settings and hope for the best. */
            context()->instance<QnWorkbenchUserWatcher>()->setUserPassword(m_newPassword);
            QUrl url = QnAppServerConnectionFactory::url();
            url.setPassword(m_newPassword);
            QnAppServerConnectionFactory::setUrl(url);

            auto connections = qnClientCoreSettings->recentUserConnections();
            if (!connections.isEmpty() &&
                !connections.first().url.password().isEmpty() &&
                qnUrlEqual(connections.first().url, url))
            {
                auto current = connections.takeFirst();
                current.url = url;
                connections.prepend(current);
                qnClientCoreSettings->setRecentUserConnections(connections);
            }

            auto lastUsed = qnSettings->lastUsedConnection();
            if (!lastUsed.url.password().isEmpty() && qnUrlEqual(lastUsed.url, url))
            {
                lastUsed.url = url;
                qnSettings->setLastUsedConnection(lastUsed);
            }

        }
    }

    if (permissions.testFlag(Qn::WriteEmailPermission))
        m_model->user()->setEmail(ui->emailInputField->text().trimmed());

    if (permissions.testFlag(Qn::WriteFullNamePermission))
        m_model->user()->setFullName(ui->nameInputField->text().trimmed());
}

bool QnUserProfileWidget::canApplyChanges() const
{
    if (m_model->mode() != QnUserSettingsModel::OwnProfile)
        return true;

    if (!ui->emailInputField->isValid())
        return false;
    return true;
}

void QnUserProfileWidget::updateControlsAccess()
{
    Qn::Permissions permissions = m_model->user()
        ? accessController()->permissions(m_model->user())
        : Qn::NoPermissions;

    /* User must confirm current password to change own password. */
    ui->changePasswordWidget->setVisible(permissions.testFlag(Qn::WritePasswordPermission));

    ::setReadOnly(ui->emailInputField, !permissions.testFlag(Qn::WriteEmailPermission));
    ::setReadOnly(ui->nameInputField, !permissions.testFlag(Qn::WriteFullNamePermission));
}

bool QnUserProfileWidget::validMode() const
{
    return m_model->mode() == QnUserSettingsModel::OwnProfile
        || m_model->mode() == QnUserSettingsModel::OtherProfile;
}
