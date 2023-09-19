// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "oauth_login_dialog_p.h"

#include <QtGui/QDesktopServices>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QStackedWidget>

#include <client/client_runtime_settings.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/async_handler_executor.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/oauth_client.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/widgets/webview_widget.h>
#include <nx/vms/client/desktop/ini.h>
#include <ui/graphics/items/standard/graphics_web_view.h>

#include "../oauth_login_dialog.h"
#include "../oauth_login_placeholder.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::client::core;

OauthLoginDialogPrivate::OauthLoginDialogPrivate(
    OauthLoginDialog* parent,
    OauthClientType clientType,
    const QString& cloudSystem)
    :
    QObject(parent),
    q(parent),
    m_stackedWidget(new QStackedWidget(parent)),
    m_progressBar(new QProgressBar(parent)),
    m_webViewWidget(new WebViewWidget(parent)),
    m_placeholder(new OauthLoginPlaceholder(parent)),
    m_oauthClient(new OauthClient(
        clientType,
        OauthViewType::desktop,
        cloudSystem))
{
    m_oauthClient->setLocale(qnRuntime->locale());

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
            if (!stopCheck())
                return;

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

bool OauthLoginDialogPrivate::stopCheck()
{
    if (m_checkClient)
    {
        m_checkClient->pleaseStopSync();
        m_checkClient.reset();
        return true;
    }
    return false;
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

    if (q->isActiveWindow())
        m_webViewWidget->setFocus();
}

void OauthLoginDialogPrivate::retryLoad()
{
    load(m_requestedUrl);
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

void OauthLoginDialogPrivate::setCode(const QString& code)
{
    m_oauthClient->setCode(code);
}

void OauthLoginDialogPrivate::twoFaVerified(const QString& code)
{
    m_oauthClient->twoFaVerified(code);
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

QString OauthLoginDialogPrivate::username()
{
    return QString::fromStdString(appContext()->coreSettings()->cloudCredentials().user);
}

OauthClient* OauthLoginDialogPrivate::oauthClient() const
{
    return m_oauthClient.get();
}

} // namespace nx::vms::client::desktop
