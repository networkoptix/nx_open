// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <memory>

#include <nx/utils/url.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>

class QLineEdit;
class QProgressBar;
class QStackedWidget;

namespace nx::cloud::db::api { class Connection; }
namespace nx::vms::client::core { class CloudConnectionFactory; }
namespace nx::network::http { class AsyncClient; }

namespace nx::vms::client::desktop {

class OauthLoginDialog;
class OauthLoginPlaceholder;
class WebViewWidget;

class OauthLoginDialogPrivate: public QObject
{
    Q_OBJECT
    OauthLoginDialog* const q;

public:
    OauthLoginDialogPrivate(
        OauthLoginDialog* parent, const QString& clientType, const QString& cloudSystem);
    virtual ~OauthLoginDialogPrivate() override;

public:
    void load(const nx::utils::Url& url);
    nx::utils::Url constructUrl(std::string_view cloudHost) const;

    /** Set token for 2FA validation. */
    void setAuthToken(const nx::network::http::AuthToken& token);

    const nx::vms::client::core::CloudAuthData& authData() const;

    // Native client methods.
    Q_INVOKABLE void setCode(const QString& code);
    Q_INVOKABLE void twoFaVerified(const QString& code);
    Q_INVOKABLE void openUrlInBrowser(const QString& path);

private:
    void issueAccessToken();

    void at_urlChanged();
    void at_loadFinished(bool success);

    void retryLoad();
    void loadInWebView(const nx::utils::Url& url);
    void displayUrl(const nx::utils::Url& url);

    void startCheck();
    void stopCheck();

    std::string email() const;

public:
    QLineEdit* m_urlLineEdit = nullptr;
    QStackedWidget* m_stackedWidget = nullptr;

private:
    const QString m_clientType;
    const QString m_cloudSystem;
    nx::utils::Url m_requestedUrl;

    QProgressBar* m_progressBar = nullptr;
    nx::vms::client::desktop::WebViewWidget* m_webViewWidget = nullptr;
    OauthLoginPlaceholder* m_placeholder = nullptr;

    nx::vms::client::core::CloudAuthData m_authData;
    std::unique_ptr<core::CloudConnectionFactory> m_cloudConnectionFactory;
    std::unique_ptr<nx::cloud::db::api::Connection> m_connection;
    std::unique_ptr<nx::network::http::AsyncClient> m_checkClient;
};

} // namespace nx::vms::client::desktop
