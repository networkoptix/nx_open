#include "camera_web_page_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <QtGui/QContextMenuEvent>
#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QNetworkCookie>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QNetworkInterface>
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

// QWebEngine does not want to connect to localhost server through localhost proxy,
// so let's try to find another address in that case.
QString getNonLocalAddress(const QString& host)
{
    if (!QHostAddress(host).isLoopback())
        return host;

    for (const QHostAddress& address: QNetworkInterface::allAddresses())
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback())
            return address.toString();
    }

    return host;
}

} // namespace

namespace nx::vms::client::desktop {

struct CameraWebPageWidget::Private
{
    Private(CameraWebPageWidget* parent);

    void loadPage();

    void resetPage();
    void createNewPage();
    void setupProxy();
    void setupCameraCookie();

    WebWidget* const webWidget;

    CameraWebPageWidget* parent;
    CameraSettingsDialogState::Credentials credentials;
    QUrl lastRequestUrl;
    QnUuid lastCameraId;
    bool loadNeeded = false;

    QnMutex mutex;
};

CameraWebPageWidget::Private::Private(CameraWebPageWidget* parent):
    webWidget(new WebWidget(parent)),
    parent(parent)
{
    auto webView = webWidget->webEngineView();
    webView->setIgnoreSslErrors(true);
    webView->setUseActionsForLinks(true);

    anchorWidgetToParent(webWidget);

    installEventHandler(parent, QEvent::Show, parent,
        [this]()
        {
            if (!loadNeeded)
                return;

            loadPage();
            loadNeeded = false;
        });
}

void CameraWebPageWidget::Private::createNewPage()
{
    auto webView = webWidget->webEngineView();
    webView->createPageWithUserAgent(kUserAgentForCameraPage);

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
        [this](const QUrl&, QAuthenticator* authenticator, const QString&)
        {
            const auto user = parent->commonModule()->currentUrl().userName();
            const auto password = parent->commonModule()->currentUrl().password();
            authenticator->setUser(user);
            authenticator->setPassword(password);
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

void CameraWebPageWidget::cleanup()
{
    QNetworkProxy::setApplicationProxy(QNetworkProxy());

    d->resetPage();
}

void CameraWebPageWidget::Private::resetPage()
{
    if (!lastRequestUrl.isValid())
        return;

    createNewPage();

    lastRequestUrl = QUrl();
    lastCameraId = {};
    loadNeeded = false;
}

void CameraWebPageWidget::Private::setupProxy()
{
    const auto currentServerUrl = parent->commonModule()->currentUrl();

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
        gatewayAddress.address.toString(), gatewayAddress.port);

    QNetworkProxy::setApplicationProxy(gatewayProxy);
}

void CameraWebPageWidget::Private::setupCameraCookie()
{
    QNetworkCookie cameraCookie(Qn::CAMERA_GUID_HEADER_NAME, lastCameraId.toByteArray());
    QUrl origin(lastRequestUrl);
    origin.setUserName(QString());
    origin.setPassword(QString());
    origin.setPath(QString());
    webWidget->webEngineView()->page()->profile()->cookieStore()->setCookie(
        cameraCookie, origin);
}

void CameraWebPageWidget::Private::loadPage()
{
    setupProxy();
    createNewPage();
    setupCameraCookie();

    webWidget->load(lastRequestUrl);
}

void CameraWebPageWidget::loadState(const CameraSettingsDialogState& state)
{
    QnMutexLocker lock(&d->mutex);
    {
        d->credentials = state.credentials;

        if (!state.canShowWebPage())
        {
            d->resetPage();
            return;
        }

        NX_ASSERT(d->credentials.login.hasValue() && d->credentials.password.hasValue());
        const auto currentServerUrl = commonModule()->currentUrl();

        const auto targetUrl = nx::network::url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName)
            .setHost(getNonLocalAddress(currentServerUrl.host()))
            .setPort(currentServerUrl.port())
            .setPath(state.singleCameraProperties.settingsUrlPath)
            .setUserName(d->credentials.login())
            .setPassword(d->credentials.password()).toUrl().toQUrl();
        NX_ASSERT(targetUrl.isValid());

        const auto cameraId = QnUuid(state.singleCameraProperties.id);

        if (d->lastRequestUrl == targetUrl && cameraId == d->lastCameraId)
            return;

        d->lastRequestUrl = targetUrl;
        d->lastCameraId = cameraId;
    }

    const bool visible = isVisible();
    d->loadNeeded = !visible;
    if (visible)
        d->loadPage();
}

} // namespace nx::vms::client::desktop
