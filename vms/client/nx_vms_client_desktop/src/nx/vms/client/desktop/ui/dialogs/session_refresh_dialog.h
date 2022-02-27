// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/auth_tools.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace nx::vms::api { struct LoginSessionRequest; }

namespace nx::vms::client::desktop {

class BusyIndicatorButton;
class InputField;

class SessionRefreshDialog: public QnSessionAwareMessageBox
{
    Q_OBJECT
    using base_type = QnSessionAwareMessageBox;

public:
    struct SessionRefreshResult
    {
        nx::network::http::AuthToken token;
        std::chrono::microseconds tokenExpirationTime;
    };

public:
    static SessionRefreshResult refreshSession(
        QWidget* parent,
        const QString& title,
        const QString& mainText,
        const QString& infoText,
        const QString& actionText,
        bool warningStyledAction,
        bool passwordValidationMode);

    SessionRefreshDialog(
        QWidget* parent,
        const QString& title,
        const QString& mainText,
        const QString& infoText,
        const QString& actionText,
        bool warningStyledAction);

    SessionRefreshResult refreshResult() const;

    /** Validate user password instead of token request. Default is false. */
    void setPasswordValidationMode(bool value);

    virtual void accept() override;

signals:
    void sessionTokenReady();

private:
    void lockUi(bool lock);
    void validationResultReady();

    void refreshSession(const nx::vms::api::LoginSessionRequest& loginRequest);
    void validatePassword(const nx::vms::api::LoginSessionRequest& loginRequest);

private:
    InputField* m_passwordField;
    BusyIndicatorButton* m_actionButton;

    bool m_passwordValidationMode = false;
    SessionRefreshResult m_refreshResult;
    ValidationResult m_validationResult = ValidationResult{QValidator::Intermediate};
};

} // nx::vms::client::desktop
