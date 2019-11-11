#include "camera_web_page_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <QtGui/QContextMenuEvent>
#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QNetworkCookie>
#include <QtNetwork/QNetworkProxy>
#include <QtWebEngineWidgets/QWebEnginePage>
#include <QtWebEngineCore/QWebEngineCookieStore>

#include <common/common_module.h>
#include <utils/common/event_processors.h>

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/web_widget.h>
#include <nx/vms/client/desktop/common/widgets/web_engine_view.h>
#include <nx/network/socket_global.h>
#include <nx/network/address_resolver.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>
#include <nx/network/url/url_builder.h>

#include <nx/cloud/vms_gateway/vms_gateway_embeddable.h>

namespace {

// This User-Agent is required for vista camera to use html/js page, not java applet.
const QString kUserAgentForCameraPage(
    "Mozilla/5.0 (Windows; U; Windows NT based; en-US)"
    " AppleWebKit/534.34 (KHTML, like Gecko)"
    "  QtWeb Internet Browser/3.8.5 http://www.QtWeb.net");

} // namespace

namespace nx::vms::client::desktop {

struct CameraWebPageWidget::Private
{
    Private(CameraWebPageWidget* parent);

    WebWidget* const webWidget;

    CameraSettingsDialogState::Credentials credentials;
    QUrl lastRequestUrl;
    bool loadNeeded = false;

    QnMutex mutex;
};

CameraWebPageWidget::Private::Private(CameraWebPageWidget* parent):
    webWidget(new WebWidget(parent))
{
    auto webView = webWidget->webEngineView();
    webView->setIgnoreSslErrors(true);
    webView->setUseActionsForLinks(true);
    webView->setUserAgent(kUserAgentForCameraPage);

    anchorWidgetToParent(webWidget);

    // Special actions list for context menu for links.
    webView->insertActions(nullptr, {
        webView->pageAction(QWebEnginePage::OpenLinkInThisWindow),
        webView->pageAction(QWebEnginePage::Copy),
        webView->pageAction(QWebEnginePage::CopyLinkToClipboard)});

    QObject::connect(webView->page(), &QWebEnginePage::authenticationRequired,
        [this](const QUrl& requestUrl, QAuthenticator* authenticator)
        {
            QnMutexLocker lock(&mutex);
            if (lastRequestUrl != requestUrl)
                return;

            NX_ASSERT(credentials.login.hasValue() && credentials.password.hasValue());
            authenticator->setUser(credentials.login());
            authenticator->setPassword(credentials.password());
    });

    QObject::connect(webView->page(), &QWebEnginePage::proxyAuthenticationRequired,
        [parent](const QUrl&, QAuthenticator* authenticator, const QString&)
        {
            const auto user = parent->commonModule()->currentUrl().userName();
            const auto password = parent->commonModule()->currentUrl().password();
            authenticator->setUser(user);
            authenticator->setPassword(password);
        });

    installEventHandler(parent, QEvent::Show, parent,
        [this]()
        {
            if (!loadNeeded)
                return;

            webWidget->load(lastRequestUrl);
            loadNeeded = false;
        });
}

CameraWebPageWidget::CameraWebPageWidget(CameraSettingsDialogStore* store, QWidget* parent):
    base_type(parent),
    QnCommonModuleAware(parent, false),
    d(new Private(this))
{
    NX_ASSERT(store);
    connect(store, &CameraSettingsDialogStore::stateChanged, this, &CameraWebPageWidget::loadState);
}

CameraWebPageWidget::~CameraWebPageWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void CameraWebPageWidget::resetApplicationProxy()
{
    QNetworkProxy::setApplicationProxy(QNetworkProxy());
}

void CameraWebPageWidget::loadState(const CameraSettingsDialogState& state)
{
    QnMutexLocker lock(&d->mutex);
    {
        d->credentials = state.credentials;

        if (!state.canShowWebPage())
        {
            d->lastRequestUrl = QUrl();
            d->webWidget->reset();
            d->loadNeeded = false;
            return;
        }

        NX_ASSERT(d->credentials.login.hasValue() && d->credentials.password.hasValue());
        const auto currentServerUrl = commonModule()->currentUrl();

        const auto targetUrl = nx::network::url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName)
            .setHost(currentServerUrl.host())
            .setPort(currentServerUrl.port())
            .setPath(state.singleCameraProperties.settingsUrlPath)
            .setUserName(d->credentials.login())
            .setPassword(d->credentials.password()).toUrl().toQUrl();
        NX_ASSERT(targetUrl.isValid());

        QNetworkCookie cameraCookie(
            Qn::CAMERA_GUID_HEADER_NAME, QnUuid(state.singleCameraProperties.id).toByteArray());
        QUrl origin(targetUrl);
        origin.setUserName(QString());
        origin.setPassword(QString());
        origin.setPath(QString());
        d->webWidget->webEngineView()->page()->profile()->cookieStore()->setCookie(
            cameraCookie, origin);

        auto gateway = nx::cloud::gateway::VmsGatewayEmbeddable::instance();

        // NOTE: Work-around to create ssl tunnel between gateway and server (not between the client
        // and server over proxy like in the normal case), because there is a bug in browser
        // implementation (or with something related) which ignores proxy settings for some
        // requests when loading camera web page. Perhaps this is not a bug, but a
        // consequence of our non-standard proxying made by the server.
        if (gateway->isSslEnabled())
        {
            int port = currentServerUrl.port();
            if (nx::network::SocketGlobals::addressResolver()
                .isCloudHostName(currentServerUrl.host()))
            {
                // NOTE: Cloud address should not have port.
                port = 0;
            }
            NX_ASSERT(port >= 0);

            gateway->enforceSslFor(
                nx::network::SocketAddress(currentServerUrl.host(), port),
                currentServerUrl.scheme() == nx::network::http::kSecureUrlSchemeName);
        }

        const auto gatewayAddress = gateway->endpoint();
        QNetworkProxy gatewayProxy(
            QNetworkProxy::HttpProxy,
            gatewayAddress.address.toString(), gatewayAddress.port,
            currentServerUrl.userName(), currentServerUrl.password());

        QNetworkProxy::setApplicationProxy(gatewayProxy);

        if (d->lastRequestUrl == targetUrl)
            return;

        NX_VERBOSE(this, "Loading state with request [%1] via proxy [%2:%3]",
            targetUrl, gatewayAddress.address.toString(), gatewayAddress.port);
        d->lastRequestUrl = targetUrl;
        d->webWidget->reset();
    }

    const bool visible = isVisible();
    d->loadNeeded = !visible;
    if (visible)
        d->webWidget->load(d->lastRequestUrl);
}

} // namespace nx::vms::client::desktop
