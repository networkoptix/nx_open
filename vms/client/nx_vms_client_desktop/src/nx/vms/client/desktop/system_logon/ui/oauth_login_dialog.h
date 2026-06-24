// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <expected>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/oauth_client_constants.h>
#include <nx/vms/client/desktop/ui/dialogs/session_refresh_data.h>
#include <nx/vms/utils/abstract_session_token_helper.h>
#include <ui/dialogs/common/dialog.h>

namespace nx::network::http { class Credentials; }
namespace nx::vms::client::core
{
    struct CloudAuthData;
    struct CloudBindData;
    struct CloudTokens;
} // namespace nx::vms::client::core

namespace nx::vms::client::desktop {

class OauthLoginDialogPrivate;

class OauthLoginDialog: public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    struct LoginParams
    {
        core::OauthClientType clientType;
        SessionRefreshFlags flags;
        QString cloudSystem;
        nx::network::http::Credentials credentials;
        std::function<bool()> closeCondition;
    };

public:
    /** Helper method for requesting fresh cloud auth data. */
    static nx::vms::client::core::CloudAuthDataOrError login(
        QWidget* parent,
        const QString& title,
        const LoginParams& params);

    /**
     * Helper method for access token 2FA validation. Returns an empty value on success, or an
     * error: `dialogRejected` when the user dismissed the dialog (Cancel, X, Esc) and
     * `internalCancellation` when it was closed by code (`OauthClient::cancelled`).
     */
    static std::expected<void, nx::vms::client::core::OauthDataRequestError> validateToken(
        QWidget* parent,
        const QString& title,
        const nx::network::http::Credentials& credentials);

    OauthLoginDialog(
        QWidget* parent,
        core::OauthClientType clientType,
        const QString& cloudSystem = QString(),
        SessionRefreshFlags flags = {});

    virtual ~OauthLoginDialog() override;

    const nx::vms::client::core::CloudAuthData& authData() const;
    const nx::vms::client::core::CloudBindData& cloudBindData() const;
    const nx::vms::client::core::CloudTokens& cloudTokens() const;

    void setCredentials(const nx::network::http::Credentials& credentials);
    void setSystemName(const QString& systenName);

signals:
    void authDataReady();
    void bindToCloudDataReady();
    void cloudTokensReady();

private:
    void loadPage();
    void rejectAsInternalCancellation();

private:
    core::OauthDataRequestError m_rejectionReason = core::OauthDataRequestError::dialogRejected;
    nx::utils::ImplPtr<OauthLoginDialogPrivate> d;
};

} // namespace nx::vms::client::desktop
