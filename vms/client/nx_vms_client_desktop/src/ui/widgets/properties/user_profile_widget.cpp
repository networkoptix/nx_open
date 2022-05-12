// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_profile_widget.h"
#include "ui_user_profile_widget.h"

#include <client_core/client_core_settings.h>
#include <client/client_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/user_resource.h>
#include <helpers/system_helpers.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_logon/logic/context_current_user_watcher.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/resource_properties/change_user_password_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/resource_properties/user_settings_model.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <utils/email/email.h>

using namespace nx::vms::client::desktop;

QnUserProfileWidget::QnUserProfileWidget(QnUserSettingsModel* model, QWidget* parent /*= 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::UserProfileWidget()),
    m_model(model),
    m_newPassword(),
    m_aligner(new Aligner(this))
{
    ui->setupUi(this);

    ::setReadOnly(ui->loginInputField, true);
    ::setReadOnly(ui->groupInputField, true);

    ui->loginInputField->setTitle(tr("Login"));
    ui->nameInputField->setTitle(tr("Name"));
    ui->groupInputField->setTitle(tr("Role"));

    ui->emailInputField->setTitle(tr("Email"));
    ui->emailInputField->setValidator(defaultEmailValidator());

    connect(ui->nameInputField, &InputField::textChanged, this,
        &QnUserProfileWidget::hasChangesChanged);
    connect(ui->emailInputField, &InputField::textChanged, this,
        &QnUserProfileWidget::hasChangesChanged);
    connect(ui->changePasswordButton, &QPushButton::clicked, this,
        &QnUserProfileWidget::editPassword);

    connect(m_model, &QnUserSettingsModel::digestSupportChanged, this,
        &QnUserProfileWidget::handleDigestSupportChange);

    m_aligner->registerTypeAccessor<InputField>(InputField::createLabelWidthAccessor());
    m_aligner->registerTypeAccessor<QnCloudUserPanelWidget>(
        QnCloudUserPanelWidget::createIconWidthAccessor());
    m_aligner->setSkipInvisible(true);

    m_aligner->addWidgets({
        ui->loginInputField,
        ui->nameInputField,
        ui->groupInputField,
        ui->changePasswordSpacerLabel,
        ui->permissionsSpacerLabel,
        ui->emailInputField,
        ui->cloudPanelWidget
    });

    autoResizePagesToContents(ui->stackedWidget,
        { QSizePolicy::Expanding, QSizePolicy::Maximum }, true);
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
    ui->groupInputField->setText(userRolesManager()->userRoleName(m_model->user()));
    ui->emailInputField->setText(m_model->user()->getEmail());
    m_newPassword.clear();

    ui->stackedWidget->setCurrentWidget(m_model->user()->isCloud()
        ? ui->cloudUserPage
        : ui->localUserPage);

    ui->cloudPanelWidget->setEnabled(m_model->user()->isEnabled());
    ui->cloudPanelWidget->setEmail(m_model->user()->getEmail());
    ui->cloudPanelWidget->setFullName(m_model->user()->fullName());

    ui->cloudPanelWidget->setManageLinkShown(
        m_model->mode() == QnUserSettingsModel::OwnProfile);

    m_aligner->align();
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

    const auto user = m_model->user();
    Qn::Permissions permissions = accessController()->permissions(user);

    //empty text means 'no change'
    if (permissions.testFlag(Qn::WritePasswordPermission) && !m_newPassword.isEmpty())
    {
        const bool isOwnProfile = m_model->mode() == QnUserSettingsModel::OwnProfile;
        if (isOwnProfile)
        {
            /* Password was changed. Change it in global settings and hope for the best. */
            context()->instance<ContextCurrentUserWatcher>()->setUserPassword(m_newPassword);
        }

        user->setPasswordAndGenerateHash(m_newPassword, m_model->digestSupport());
    }

    if (permissions.testFlag(Qn::WriteEmailPermission))
        user->setEmail(ui->emailInputField->text().trimmed());

    if (permissions.testFlag(Qn::WriteFullNamePermission))
        user->setFullName(ui->nameInputField->text().trimmed());
}

bool QnUserProfileWidget::canApplyChanges() const
{
    if (m_model->mode() != QnUserSettingsModel::OwnProfile)
        return true;

    if (!ui->emailInputField->isValid())
        return false;
    return true;
}

QString QnUserProfileWidget::requestNewPassword(const QString& reason)
{
    const auto permissions = accessController()->permissions(m_model->user());
    if (!permissions.testFlag(Qn::WritePasswordPermission))
        return {};

    const QScopedPointer<QnChangeUserPasswordDialog> dialog(new QnChangeUserPasswordDialog(this));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setInfoText(reason);
    if (dialog->exec() != QDialog::Accepted)
        return {};

    return dialog->newPassword();
}

void QnUserProfileWidget::editPassword()
{
    if (!validMode())
        return;

    const auto newPassword = requestNewPassword();
    if (newPassword.isEmpty()) //< Dialog was not accepted.
        return;
    if (m_newPassword == newPassword)
        return;

    m_newPassword = newPassword;
    emit hasChangesChanged();
}

void QnUserProfileWidget::handleDigestSupportChange()
{
    if (!validMode())
        return;

    if (!m_newPassword.isEmpty()) //< The password has been set already.
        return;

    if (m_model->digestSupport() == QnUserResource::DigestSupport::keep)
        return;

    if (m_model->digestAuthorizationEnabled())
    {
        const auto password =
            requestNewPassword(tr("To enable digest authentication password reset is required"));
        if (password.isEmpty())
        {
            // Revert the model state.
            m_model->setDigestAuthorizationEnabled(!m_model->digestAuthorizationEnabled());
            return;
        }
        m_newPassword = password;
    }

    emit hasChangesChanged();
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
