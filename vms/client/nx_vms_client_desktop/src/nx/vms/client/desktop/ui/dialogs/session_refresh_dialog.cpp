// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "session_refresh_dialog.h"

#include <QtWidgets/QLabel>

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/url.h>
#include <nx/vms/auth/auth_result.h>
#include <nx/vms/client/desktop/common/widgets/busy_indicator_button.h>
#include <nx/vms/client/desktop/common/widgets/password_input_field.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_logon/data/logon_data.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/dialogs/session_refresh_dialog.h>
#include <nx/vms/utils/system_uri.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

SessionRefreshDialog::SessionRefreshResult SessionRefreshDialog::refreshSession(
    QWidget* parent,
    const QString& title,
    const QString& mainText,
    const QString& infoText,
    const QString& actionText,
    bool warningStyledAction,
    bool passwordValidationMode,
    Qt::WindowFlags flags)
{
    SessionRefreshDialog dialog(
        parent, title, mainText, infoText, actionText, warningStyledAction, flags);

    dialog.setPasswordValidationMode(passwordValidationMode);
    connect(
        &dialog,
        &SessionRefreshDialog::sessionTokenReady,
        &dialog,
        &SessionRefreshDialog::accept);

    dialog.exec();
    return dialog.refreshResult();
}

SessionRefreshDialog::SessionRefreshDialog(
    QWidget* parent,
    const QString& title,
    const QString& mainText,
    const QString& infoText,
    const QString& actionText,
    bool warningStyledAction,
    Qt::WindowFlags flags)
    :
    base_type(parent)
{
    setWindowTitle(title);
    setIcon(Icon::Warning);
    setText(mainText);
    setInformativeText(infoText);
    setWindowFlags(windowFlags() | flags);

    if (context()->user()->isTemporary())
    {
        auto loginLabel = new QLabel(tr("Access Link"), this);
        addCustomWidget(loginLabel);

        m_linkField = new InputField(this);

        m_linkField->setValidator([this](const QString& /*link*/)
            {
                return m_validationResult;
            });

        addCustomWidget(
            m_linkField,
            QnMessageBox::Layout::Content,
            /*stretch*/ 0,
            Qt::Alignment(),
            /*focusThisWidget*/ true);
    }
    else
    {
        // Login input(disabled) and label;
        auto loginLabel = new QLabel(tr("Login"), this);
        addCustomWidget(loginLabel);

        auto loginField = new InputField(this);
        loginField->setEnabled(false);
        loginField->setText(QString::fromStdString(systemContext()->connectionCredentials().username));
        addCustomWidget(loginField);

        // Password input and label.
        auto passwordLabel = new QLabel(tr("Password"), this);
        addCustomWidget(passwordLabel);

        m_passwordField = new PasswordInputField(this);
        m_passwordField->setValidator(
            [this](const QString& /*password*/)
            {
                return m_validationResult;
            });
        addCustomWidget(
            m_passwordField,
            QnMessageBox::Layout::Content,
            /*stretch*/ 0,
            Qt::Alignment(),
            /*focusThisWidget*/ true);
    }

    // Action button.
    m_actionButton = new BusyIndicatorButton(this);
    m_actionButton->setText(actionText);
    if (warningStyledAction)
        setWarningButtonStyle(m_actionButton);
    addButton(m_actionButton, QDialogButtonBox::AcceptRole);

    // Cancel button.
    setStandardButtons({QDialogButtonBox::Cancel});
}

void SessionRefreshDialog::accept()
{
    if (!m_refreshResult.token.empty())
    {
        base_type::accept();
        return;
    }

    refreshSession();
}

void SessionRefreshDialog::refreshSession()
{
    auto callback = nx::utils::guarded(
        this,
        [this](
            bool success,
            int reqId,
            rest::ErrorOrData<nx::vms::api::LoginSession> errorOrData)
        {
            if (auto session = std::get_if<nx::vms::api::LoginSession>(&errorOrData))
            {
                NX_DEBUG(this, "Received token with length: %1", session->token.length());
                
                if (context()->user()->isTemporary() && context()->user()->getId() != session->id)
                {
                    desktop::LogonData logonData;
                    logonData.address = m_address;
                    logonData.credentials = {session->username.toStdString(), m_token};
                    logonData.userType = nx::vms::api::UserType::temporaryLocal;
                    logonData.storeSession = false;
                    menu()->trigger(ui::action::ConnectAction,
                        ui::action::Parameters().withArgument(Qn::LogonDataRole, logonData));
                    base_type::reject();
                }
                else if (NX_ASSERT(!session->token.empty()))
                {
                    m_validationResult = ValidationResult::kValid;
                    m_refreshResult.token = nx::network::http::BearerAuthToken(session->token);
                    m_refreshResult.tokenExpirationTime =
                        qnSyncTime->currentTimePoint() + session->expiresInS;

                    if (m_passwordField)
                        m_refreshResult.password = m_passwordField->text();
                }
                else
                {
                    m_validationResult = ValidationResult(QValidator::Invalid);
                }
            }
            else
            {
                auto error = std::get_if<nx::network::rest::Result>(&errorOrData);
                NX_INFO(this, "Can't receive token: %1", QJson::serialized(*error));
                m_validationResult = ValidationResult(QValidator::Invalid, error->errorString);
            }

            validationResultReady();
        });

    if (context()->user()->isTemporary())
    {

        const auto uri = nx::vms::utils::SystemUri::fromTemporaryUserLink(m_linkField->text());

        if (!uri.isValid()
            || uri.userAuthType != nx::vms::utils::SystemUri::UserAuthType::temporary)
        {
            m_validationResult = ValidationResult(QValidator::Invalid, tr("Invalid Link"));
            return;
        }

        nx::vms::api::TemporaryLoginSessionRequest loginRequest{uri.authKey()};
        m_address = uri.systemAddress.toStdString();
        m_token = uri.credentials.authToken;

        lockUi(true);
        if (auto api = connectedServerApi(); NX_ASSERT(api, "No Server connection"))
            api->loginAsync(loginRequest, std::move(callback), thread());
        else
            base_type::reject();
    }
    else
    {
        nx::vms::api::LoginSessionRequest loginRequest;
        loginRequest.username = QString::fromStdString(
            systemContext()->connectionCredentials().username);
        loginRequest.password = m_passwordField->text();

        lockUi(true);

        if (m_passwordValidationMode)
            validatePassword(loginRequest);
        else if (auto api = connectedServerApi(); NX_ASSERT(api, "No Server connection"))
            api->loginAsync(loginRequest, std::move(callback), thread());
        else
            base_type::reject();
    }
}

void SessionRefreshDialog::validatePassword(const nx::vms::api::LoginSessionRequest& loginRequest)
{
    const nx::network::http::AuthToken currentToken =
        systemContext()->connectionCredentials().authToken;

    NX_ASSERT(currentToken.isPassword());
    if (loginRequest.password == currentToken.value)
    {
        m_validationResult = ValidationResult::kValid;
        m_refreshResult.token = currentToken;
        m_refreshResult.tokenExpirationTime = {};
    }
    else
    {
        m_validationResult = ValidationResult(
            QValidator::Invalid,
            nx::vms::common::toErrorMessage(nx::vms::common::Auth_WrongPassword));
    }

    validationResultReady();
}

void SessionRefreshDialog::lockUi(bool lock)
{
    const bool enabled = !lock;

    if (m_passwordField)
        m_passwordField->setEnabled(enabled);

    m_actionButton->setEnabled(enabled);
    m_actionButton->showIndicator(lock);
}

void SessionRefreshDialog::validationResultReady()
{
    if (m_validationResult.state == QValidator::Acceptable)
    {
        emit sessionTokenReady();
    }
    else
    {
        if (m_validationResult.errorMessage.isEmpty())
            m_validationResult.errorMessage = tr("Unknown error");

        if (m_passwordField)
        {
            m_passwordField->clear();
            m_passwordField->validate();
        }

        if (m_linkField)
        {
            m_validationResult.errorMessage = tr("The provided link is not valid");
            m_linkField->validate();
        }
        // Reset validation result after displaying error message.
        m_validationResult = ValidationResult(QValidator::Intermediate);

        lockUi(false);
    }
}

SessionRefreshDialog::SessionRefreshResult SessionRefreshDialog::refreshResult() const
{
    return m_refreshResult;
}

void SessionRefreshDialog::setPasswordValidationMode(bool value)
{
    m_passwordValidationMode = value;
}

} // nx::vms::client::desktop
