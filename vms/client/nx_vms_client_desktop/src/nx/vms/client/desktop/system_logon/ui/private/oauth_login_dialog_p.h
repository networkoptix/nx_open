// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <nx/utils/url.h>
#include <nx/vms/client/core/network/cloud_auth_data.h>
#include <nx/vms/client/core/network/oauth_client_constants.h>

class QLineEdit;
class QProgressBar;
class QStackedWidget;

namespace nx::cloud::db::api { class Connection; }
namespace nx::network::http { class AsyncClient; }

namespace nx::vms::client::core {

class CloudConnectionFactory;
class OauthClient;

} // namespace nx::vms::client::core

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
        OauthLoginDialog* parent,
        core::OauthClientType clientType,
        const QString& cloudSystem);

    virtual ~OauthLoginDialogPrivate() override;

public:
    void load(const nx::utils::Url& url);

    // Native client methods.
    Q_INVOKABLE void setCode(const QString& code);

    Q_INVOKABLE void setBindInfo(const QJsonObject& jsonObject);
    Q_INVOKABLE void setTokens(const QJsonObject& jsonObject);

    Q_INVOKABLE void twoFaVerified(const QString& code);
    Q_INVOKABLE void openUrlInBrowser(const QString& path);
    Q_INVOKABLE QString username();

    core::OauthClient * oauthClient() const;

private:
    void at_urlChanged();
    void at_loadFinished(bool success);

    void retryLoad();
    void loadInWebView(const nx::utils::Url& url);
    void displayUrl(const nx::utils::Url& url);

    void startCheck();
    bool stopCheck();

public:
    QLineEdit* m_urlLineEdit = nullptr;
    QStackedWidget* m_stackedWidget = nullptr;

private:
    nx::utils::Url m_requestedUrl;

    QProgressBar* m_progressBar = nullptr;
    WebViewWidget* m_webViewWidget = nullptr;
    OauthLoginPlaceholder* m_placeholder = nullptr;

    std::unique_ptr<nx::network::http::AsyncClient> m_checkClient;
    std::unique_ptr<core::OauthClient> m_oauthClient;
};

} // namespace nx::vms::client::desktop
