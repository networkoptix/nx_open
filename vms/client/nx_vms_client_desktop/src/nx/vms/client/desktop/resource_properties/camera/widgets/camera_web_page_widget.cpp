#include "camera_web_page_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <chrono>

#include <QtCore/QElapsedTimer>
#include <QtCore/QRegExp>
#include <QtGui/QContextMenuEvent>
#include <QtWidgets/QAction>
#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QNetworkCookie>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QNetworkInterface>
#include <QtWebEngineWidgets/QWebEnginePage>
#include <QtWebEngineWidgets/QWebEngineScript>
#include <QtWebEngineWidgets/QWebEngineScriptCollection>
#include <QtWebEngineCore/QWebEngineCookieStore>
#include <QtWebEngineCore/QWebEngineUrlRequestInterceptor>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

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

#include <ui/dialogs/common/password_dialog.h>

using namespace std::chrono;

namespace {

constexpr milliseconds kHttpAuthSmallInterval = 10s;
constexpr int kHttpAuthDialogAttemptsLimit = 3;

/**
 * Allows to monitor if certain number of attemps is exceeded withing a time perdiod.
 */
class AttemptCounter
{
public:
    AttemptCounter() { setLimit(1); }

    void reset()
    {
        m_next = 0;
        m_count = 0;
    }

    void setLimit(size_t limit)
    {
        reset();

        if (limit == 0)
            return;

        m_timeStamps.resize(limit, {});
    }

    size_t limit() const { return m_timeStamps.size(); }

    size_t count() const { return m_count; }

    void registerAttempt()
    {
        if (m_count < limit())
            ++m_count;

        m_timeStamps[m_next] = steady_clock::now();
        m_next = (m_next + 1) % limit();
    }

    milliseconds timeFrame() const
    {
        if (m_count == 0)
            return milliseconds::zero();

        size_t first = m_next % limit();
        size_t last = m_next == 0 ? m_count - 1 : m_next - 1;

        return duration_cast<milliseconds>(m_timeStamps[last] - m_timeStamps[first]);
    }

    bool exceeded(milliseconds time) const
    {
        return m_count == limit() && timeFrame() < time;
    }

private:
    // Ring buffer.
    std::vector<steady_clock::time_point> m_timeStamps;
    size_t m_next = 0;
    size_t m_count = 0;
};

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

bool currentlyOnUiThread()
{
    return QThread::currentThread() == qApp->thread();
}

// Accoding to the docs, in Qt WebEngine, the QAuthenticator
// must be explicitly set to null to cancel authentication.
void cancelAuthentication(QAuthenticator* authenticator)
{
    *authenticator = QAuthenticator();
}

// Redirects GET request made by camera page if it is made using camera local IP address.
class RequestUrlFixupInterceptor: public QWebEngineUrlRequestInterceptor
{
    using base_type = QWebEngineUrlRequestInterceptor;

public:
    RequestUrlFixupInterceptor(QObject* p, const QUrl& serverUrl, const QString& ipAddress):
        base_type(p),
        m_serverUrl(serverUrl),
        m_ipAddress(ipAddress)
    {
    }

    virtual void interceptRequest(QWebEngineUrlRequestInfo& info) override
    {
        // Camera wants to make requests using its local network IP address.
        // Redirect such requests to the proxying server.

        // Only GET request can be redirected.
        if (info.requestMethod() != "GET")
            return;

        const auto pageHost = info.firstPartyUrl().host();

        // Server or camera page makes the request.
        if (pageHost != m_serverUrl.host() && pageHost != m_ipAddress)
            return;

        // Requested host matches camera IP address.
        if (info.requestUrl().host() != m_ipAddress)
            return;

        // Redirect the request through the server.
        QUrl redirectUrl = info.requestUrl();
        redirectUrl.setHost(m_serverUrl.host());
        redirectUrl.setPort(m_serverUrl.port());
        info.redirect(redirectUrl);
    }

private:
    QUrl m_serverUrl;
    QString m_ipAddress;
};

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
    CameraSettingsDialogState::SingleCameraProperties lastCamera;
    bool loadNeeded = false;

    AttemptCounter authDialodCounter;
    QUrl testPageUrl;
};

CameraWebPageWidget::Private::Private(CameraWebPageWidget* parent):
    webWidget(new WebWidget(parent)),
    parent(parent)
{
    // For testing purposes only.
    const QByteArray testPageUrlValue = qgetenv("VMS_CAMERA_SETTINGS_WEB_PAGE_URL");
    if (!testPageUrlValue.isEmpty())
        testPageUrl = QUrl(QString::fromLocal8Bit(testPageUrlValue));

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
    webView->createPageWithNewProfile();

    if (!lastCamera.overrideHttpUserAgent.isNull())
        webView->page()->profile()->setHttpUserAgent(lastCamera.overrideHttpUserAgent);

    webView->page()->profile()->clearHttpCache();

    if (lastCamera.overrideXmlHttpRequestTimeout > 0)
    {
        // Inject script that overrides the timeout before sending the XMLHttpRequest.
        QWebEngineScript script;
        const QString s = QString::fromUtf8(R"JS(
            (function() {
                XMLHttpRequest.prototype.realSend = XMLHttpRequest.prototype.send;
                var newSend = function(data) {
                    if (this.timeout !== 0 && this.timeout < %1)
                        this.timeout = %1;
                    this.realSend(data);
                };
                XMLHttpRequest.prototype.send = newSend;
            })()
            )JS").arg(lastCamera.overrideXmlHttpRequestTimeout);
        script.setName("overrideXmlHttpRequestTimeout");
        script.setSourceCode(s);
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setRunsOnSubFrames(true);
        script.setWorldId(QWebEngineScript::MainWorld);
        webView->page()->profile()->scripts()->insert(script);
    }

    if (lastCamera.fixupRequestUrls)
    {
        // Inject script that replaces camera host with server host
        // in the request url before sending the XMLHttpRequest.
        QWebEngineScript script;
        const QString s = QString::fromUtf8(R"JS(
            (function() {
                XMLHttpRequest.prototype.realOpen = XMLHttpRequest.prototype.open;
                var newOpen = function(method, url, async, user, password) {
                    var newUrl = url.replace(/^(https?:\/\/)?(%1)(:\d+)?(.*)/g, '$1' + '%2:%3' + '$4');
                    this.realOpen(method, newUrl, async, user, password);
                };
                XMLHttpRequest.prototype.open = newOpen;
            })()
            )JS")
            .arg(QRegExp::escape(lastCamera.ipAddress))
            .arg(lastRequestUrl.host())
            .arg(lastRequestUrl.port());
        script.setName("fixupXmlHttpRequestUrl");
        script.setSourceCode(s);
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setRunsOnSubFrames(true);
        script.setWorldId(QWebEngineScript::MainWorld);
        webView->page()->profile()->scripts()->insert(script);
    }

    // Special actions list for context menu for links.
    webView->insertActions(nullptr, {
        webView->pageAction(QWebEnginePage::OpenLinkInThisWindow),
        webView->pageAction(QWebEnginePage::Copy)});

    webView->setHiddenActions({
        QWebEnginePage::DownloadImageToDisk,
        QWebEnginePage::DownloadLinkToDisk,
        QWebEnginePage::DownloadMediaToDisk,
        QWebEnginePage::SavePage,
        QWebEnginePage::CopyLinkToClipboard,
        QWebEnginePage::CopyImageUrlToClipboard,
        QWebEnginePage::CopyMediaUrlToClipboard});

    authDialodCounter.setLimit(kHttpAuthDialogAttemptsLimit);

    QObject::connect(webView->page(), &QWebEnginePage::authenticationRequired,
        [this](const QUrl& requestUrl, QAuthenticator* authenticator)
        {
            NX_ASSERT(currentlyOnUiThread());

            // Camera may redirect to another path and ask for credentials,
            // so check for host match.
            if (lastRequestUrl.host() == requestUrl.host())
            {
                const auto camera = parent->commonModule()
                                        ->resourcePool()
                                        ->getResourceById<QnVirtualCameraResource>(
                                            QnUuid::fromStringSafe(lastCamera.id));
                if (camera && camera->getStatus() != Qn::Unauthorized)
                {
                    authenticator->setUser(credentials.login());
                    authenticator->setPassword(credentials.password());
                    return;
                }
            }

            authDialodCounter.registerAttempt();
            if (authDialodCounter.exceeded(kHttpAuthSmallInterval))
            {
                cancelAuthentication(authenticator);
                return;
            }

            PasswordDialog dialog(parent);

            auto url = requestUrl;

            // Hide credentials.
            url.setUserName(QString());
            url.setPassword(QString());

            // Replace server address with camera address.
            if (url.host() == lastRequestUrl.host())
            {
                url.setHost(lastCamera.ipAddress);
                url.setPort(-1); //< Hide server port.
            }

            dialog.setText(url.toString());

            if (dialog.exec() == QDialog::Accepted)
            {
                authenticator->setUser(dialog.username());
                authenticator->setPassword(dialog.password());
            }
            else
            {
                cancelAuthentication(authenticator);
            }
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

void CameraWebPageWidget::keyPressEvent(QKeyEvent* event)
{
    base_type::keyPressEvent(event);

    // Web page JavaScript code observes and reacts on key presses, but may not accept the actual
    // event. In regular web browser pressing those keys does nothing, but in camera settings web
    // page dialog they will close the dialog and the user won't be able interact with the web
    // page any further.
    // So just accept them anyway in order to provide the same user experience as in regular
    // web browser.
    switch (event->key())
    {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Escape:
            event->setAccepted(true);
            break;
        default:
            break;
    }
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
    lastCamera = {};
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
    // Using session key cookie here to avoid PROXY_UNAUTHORIZED from server after HTTP CONNECT to
    // the gateway which is not standard-compliant behaviour (see: VMS-16773).
    QNetworkCookie sessionKeyCookie(Qn::EC2_RUNTIME_GUID_HEADER_NAME.toLower(),
        parent->commonModule()->runningInstanceGUID().toByteArray());
    QNetworkCookie cameraCookie(Qn::CAMERA_GUID_HEADER_NAME, lastCamera.id.toUtf8());

    QUrl origin(lastRequestUrl);
    origin.setUserName(QString());
    origin.setPassword(QString());
    origin.setPath(QString());

    auto profile = webWidget->webEngineView()->page()->profile();
    profile->cookieStore()->setCookie(cameraCookie, origin);
    profile->cookieStore()->setCookie(sessionKeyCookie, origin);

    if (lastCamera.fixupRequestUrls)
    {
        auto interceptor =
            new RequestUrlFixupInterceptor(profile, lastRequestUrl, lastCamera.ipAddress);
        profile->setUrlRequestInterceptor(interceptor);
    }
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
    NX_ASSERT(currentlyOnUiThread());

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
        .setPath(state.singleCameraProperties.settingsUrlPath).toUrl().toQUrl();
    NX_ASSERT(targetUrl.isValid());

    const auto cameraId = state.singleCameraProperties.id;

    if (d->lastRequestUrl == targetUrl && cameraId == d->lastCamera.id)
        return;

    d->lastRequestUrl = d->testPageUrl.isValid() ? d->testPageUrl : targetUrl;
    d->lastCamera = state.singleCameraProperties;

    const bool visible = isVisible();
    d->loadNeeded = !visible;
    if (visible)
        d->loadPage();
}

} // namespace nx::vms::client::desktop
