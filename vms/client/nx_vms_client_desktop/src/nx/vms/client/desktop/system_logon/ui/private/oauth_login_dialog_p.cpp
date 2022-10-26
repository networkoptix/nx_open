// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth_login_dialog_p.h"

#include <QtGui/QDesktopServices>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QStackedWidget>

#include <client/client_runtime_settings.h>
#include <client_core/client_core_module.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/cloud_connection_factory.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/utils/cloud_session_token_updater.h>
#include <nx/vms/client/desktop/common/widgets/webview_widget.h>
#include <nx/vms/client/desktop/ini.h>
#include <ui/graphics/items/standard/graphics_web_view.h>
#include <watchers/cloud_status_watcher.h>

#include "../oauth_login_dialog.h"
#include "../oauth_login_placeholder.h"

using namespace nx::cloud::db::api;

namespace nx::vms::client::desktop {

OauthLoginDialogPrivate::OauthLoginDialogPrivate(
    OauthLoginDialog* parent, const QString& clientType, const QString& cloudSystem)
    :
    QObject(parent),
    q(parent),
    m_stackedWidget(new QStackedWidget(parent)),
    m_clientType(clientType),
    m_cloudSystem(cloudSystem),
    m_progressBar(new QProgressBar(parent)),
    m_webViewWidget(new WebViewWidget(parent)),
    m_placeholder(new OauthLoginPlaceholder(parent)),
    m_cloudConnectionFactory(std::make_unique<core::CloudConnectionFactory>())
{
    connect(
        m_webViewWidget->controller(),
        &WebViewController::windowCloseRequested,
        q,
        &OauthLoginDialog::accept);

    connect(
        m_webViewWidget->controller(),
        &WebViewController::urlChanged,
        this,
        &OauthLoginDialogPrivate::at_urlChanged);

    connect(m_webViewWidget->controller(), &WebViewController::loadFinished,
        this, &OauthLoginDialogPrivate::at_loadFinished);

    m_webViewWidget->controller()->setRedirectLinksToDesktop(true);
    m_webViewWidget->controller()->setMenuNavigation(false);

    static const QString kExportedName("nativeClient");
    m_webViewWidget->controller()->registerObject(kExportedName, this);

    if (ini().developerMode)
    {
        m_urlLineEdit = new QLineEdit(q);
        connect(
            m_urlLineEdit,
            &QLineEdit::returnPressed,
            this,
            [this]() { load(m_urlLineEdit->text()); });
    }

    m_progressBar->setStyleSheet(QString::fromUtf8(R"(
        QProgressBar {
            border-top: 1px;
            border-bottom: 1px;
            border-style: outset;
        }
        )"));
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 0);

    connect(m_placeholder, &OauthLoginPlaceholder::retryRequested,
        this, &OauthLoginDialogPrivate::retryLoad);

    m_stackedWidget->addWidget(m_progressBar);
    m_stackedWidget->addWidget(m_webViewWidget);
    m_stackedWidget->addWidget(m_placeholder);

    m_stackedWidget->setCurrentWidget(m_progressBar);
}

OauthLoginDialogPrivate::~OauthLoginDialogPrivate()
{
    stopCheck();
}

std::string OauthLoginDialogPrivate::email() const
{
    auto email = m_authData.credentials.username;

    if (email.empty())
    {
        nx::vms::client::core::RemoteConnectionAware connectionHelper;
        auto connection = connectionHelper.connection();

        if (connection && connection->connectionInfo().userType == nx::vms::api::UserType::cloud)
            email = connection->credentials().username;
    }

    if (email.empty())
        email = qnCloudStatusWatcher->cloudLogin().toStdString();

    return email;
}

nx::utils::Url OauthLoginDialogPrivate::constructUrl(std::string_view cloudHost) const
{
    auto builder = nx::network::url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setHost(cloudHost)
        .setPath("authorize")
        .addQueryItem("client_type", m_clientType)
        .addQueryItem("client_id", "desktopclient")
        .addQueryItem("view_type", "desktop")
        .addQueryItem("redirect_url", "redirect-oauth")
        .addQueryItem("lang", qnRuntime->locale());

    if (m_authData.empty()) //< Request auth code.
        builder.addQueryItem("response_type", "code");
    else if (!m_authData.credentials.authToken.empty()) //< Request 2FA validation.
        builder.addQueryItem("access_token", m_authData.credentials.authToken.value);

    if (const auto email = this->email(); !email.empty())
        builder.addQueryItem("email", email);

    return builder.toUrl();
}


void OauthLoginDialogPrivate::load(const nx::utils::Url& url)
{
    m_requestedUrl = url;
    m_stackedWidget->setCurrentWidget(m_progressBar);
    displayUrl(url);
    startCheck();
}

void OauthLoginDialogPrivate::startCheck()
{
    m_checkClient = std::make_unique<nx::network::http::AsyncClient>(
        nx::network::ssl::kDefaultCertificateCheck);
    auto callback = nx::utils::AsyncHandlerExecutor(this).bind(
        [this](bool success)
        {
            stopCheck();
            NX_DEBUG(this, "Ping result: %1, url: %2", success, m_requestedUrl);

            if (success)
                loadInWebView(m_requestedUrl);
            else
                m_stackedWidget->setCurrentWidget(m_placeholder);
        });
    auto clientCallback =
        [client = m_checkClient.get(), callback = std::move(callback)]() mutable
        {
            callback(client->hasRequestSucceeded());
        };

    m_checkClient->setOnResponseReceived(clientCallback);
    m_checkClient->setOnDone(clientCallback);
    m_checkClient->doGet(m_requestedUrl);
}

void OauthLoginDialogPrivate::stopCheck()
{
    if (m_checkClient)
    {
        m_checkClient->pleaseStopSync();
        m_checkClient.reset();
    }
}

void OauthLoginDialogPrivate::loadInWebView(const nx::utils::Url& url)
{
    const QString urlStr = url.toString(QUrl::RemovePassword | QUrl::FullyEncoded);
    NX_DEBUG(this, "Opening webview URL: %1", urlStr);

    const QString script = QString("window.location.replace(\'%1\')").arg(urlStr);
    m_webViewWidget->controller()->runJavaScript(script);
}

void OauthLoginDialogPrivate::at_loadFinished(bool success)
{
    m_stackedWidget->setCurrentWidget(success
        ? static_cast<QWidget*>(m_webViewWidget)
        : m_placeholder);
}

const nx::vms::client::core::CloudAuthData& OauthLoginDialogPrivate::authData() const
{
    return m_authData;
}

void OauthLoginDialogPrivate::setAuthToken(const nx::network::http::AuthToken& token)
{
    nx::vms::client::core::CloudAuthData authData;
    authData.credentials.authToken = token;
    m_authData = std::move(authData);
}

void OauthLoginDialogPrivate::retryLoad()
{
    load(m_requestedUrl);
}

void OauthLoginDialogPrivate::setCode(const QString& code)
{
    NX_DEBUG(this, "Auth code set, length: %1", code.size());
    if (code.isEmpty())
    {
        q->reject();
        return;
    }

    m_authData.authorizationCode = code.toStdString();
    issueAccessToken();
}

void OauthLoginDialogPrivate::twoFaVerified(const QString& code)
{
    NX_DEBUG(this, "2FA code verified, length: %1", code.size());

    if (!NX_ASSERT(!code.isEmpty() && (code == m_authData.authorizationCode
        || code == m_authData.credentials.authToken.value)))
    {
        q->reject();
        return;
    }

    emit q->authDataReady();
}

void OauthLoginDialogPrivate::at_urlChanged()
{
    auto url = nx::utils::Url::fromQUrl(m_webViewWidget->controller()->url());
    NX_DEBUG(this, "Url changed: %1", url);
    displayUrl(url);
}

void OauthLoginDialogPrivate::displayUrl(const nx::utils::Url& url)
{
    if (m_urlLineEdit)
    {
        m_urlLineEdit->setText(url.toString());
        m_urlLineEdit->setSelection(0, 0);
    }
}

void OauthLoginDialogPrivate::issueAccessToken()
{
    m_connection = m_cloudConnectionFactory->createConnection();
    if (!NX_ASSERT(m_connection))
    {
        q->reject();
        return;
    }

    auto handler = nx::utils::AsyncHandlerExecutor(this).bind(
        [this](ResultCode result, IssueTokenResponse response)
        {
            NX_DEBUG(
                this,
                "Issue token result: %1, error: %2",
                result,
                response.error.value_or(std::string()));

            if (result != ResultCode::ok)
            {
                q->reject();
                return;
            }

            m_authData.authorizationCode.clear();
            m_authData.credentials = nx::network::http::BearerAuthToken(response.access_token);
            m_authData.refreshToken = std::move(response.refresh_token);
            m_authData.expiresAt = response.expires_at;
            emit q->authDataReady();
        });

    IssueTokenRequest request;
    request.grant_type = IssueTokenRequest::GrantType::authorizationCode;
    request.code = m_authData.authorizationCode;
    if (!m_cloudSystem.isEmpty())
        request.scope = nx::format("cloudSystemId=%1", m_cloudSystem).toStdString();

    m_connection->oauthManager()->issueToken(request, std::move(handler));
}

void OauthLoginDialogPrivate::openUrlInBrowser(const QString &path)
{
    nx::utils::Url externalUrl(path);
    if (!externalUrl.isValid() || externalUrl.scheme().isEmpty())
    {
        const QUrl currentUrl = m_webViewWidget->controller()->url();
        externalUrl = nx::network::url::Builder()
            .setScheme(currentUrl.scheme())
            .setHost(currentUrl.host())
            .setPort(currentUrl.port())
            .setPath(path)
            .toUrl();
    }

    NX_INFO(this, "External URL requested: %1", externalUrl);

    if (!NX_ASSERT(externalUrl.isValid()))
    {
        NX_WARNING(this, "External URL is invalid: %1", externalUrl);
        return;
    }

    QDesktopServices::openUrl(externalUrl.toQUrl());
}

} // namespace nx::vms::client::desktop
