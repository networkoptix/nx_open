#include "camera_web_page_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <QtGui/QContextMenuEvent>
#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkCookieJar>
#include <QtNetwork/QNetworkCookie>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QNetworkReply>
#include <QtWebKitWidgets/QWebFrame>
#include <QtWebKitWidgets/QWebPage>
#include <QtWebKitWidgets/QWebView>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineCookieStore>

#include <common/common_module.h>
#include <ui/widgets/common/web_page.h>
#include <utils/common/event_processors.h>

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/web_widget.h>
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

} // anonymous namespace

namespace nx::vms::client::desktop {

struct CameraWebPageWidget::Private
{
    Private(CameraWebPageWidget* parent);

    class WebPage;
    class WebEnginePage;
    class CookieJar;

    WebWidget* const webWidget;
    const QScopedPointer<CookieJar> cookieJar;

    CameraSettingsDialogState::Credentials credentials;
    QNetworkRequest lastRequest;
    bool loadNeeded = false;
    QScopedPointer<QWebEngineProfile> webEngineProfile;

    QnMutex mutex;
};

class CameraWebPageWidget::Private::CookieJar: public QNetworkCookieJar
{
    using base_type = QNetworkCookieJar;

public:
    using base_type::base_type; //< Forward constructors.

    void setCameraId(const QnUuid& uuid)
    {
        m_cameraId = uuid.toByteArray();
    }

    virtual QList<QNetworkCookie> cookiesForUrl(const QUrl& /*url*/) const override
    {
        if (m_cameraId.isEmpty())
            return {};

        return {QNetworkCookie(Qn::CAMERA_GUID_HEADER_NAME, m_cameraId)};
    }

private:
    QByteArray m_cameraId;
};

class CameraWebPageWidget::Private::WebPage: public QnWebPage
{
    using base_type = QnWebPage;

public:
    using base_type::base_type; //< Forward constructors.

protected:
    virtual QString userAgentForUrl(const QUrl& /*url*/) const
    {
        return kUserAgentForCameraPage;
    }
};

class CameraWebPageWidget::Private::WebEnginePage: public QWebEnginePage
{
    using base_type = QWebEnginePage;

public:
    using base_type::base_type; //< Forward constructors.

protected:
    virtual bool certificateError(const QWebEngineCertificateError&) override
    {
        // Ignore the error and complete the request.
        return true;
    }
};

CameraWebPageWidget::Private::Private(CameraWebPageWidget* parent):
    webWidget(new WebWidget(parent)),
    cookieJar(new CookieJar())
{
    if (auto webView = webWidget->view())
    {
        webView->setPage(new WebPage(webView));

        anchorWidgetToParent(webWidget);

        // Special actions list for context menu for links.
        webView->insertActions(nullptr, {
            webView->pageAction(QWebPage::OpenLink),
            webView->pageAction(QWebPage::Copy),
            webView->pageAction(QWebPage::CopyLinkToClipboard)});

        const auto accessManager = webView->page()->networkAccessManager();
        accessManager->setCookieJar(cookieJar.data());

        QObject::connect(accessManager, &QNetworkAccessManager::authenticationRequired,
            [this](QNetworkReply* reply, QAuthenticator* authenticator)
            {
                QnMutexLocker lock(&mutex);
                if (lastRequest != reply->request())
                    return;

                NX_ASSERT(credentials.login.hasValue() && credentials.password.hasValue());
                authenticator->setUser(credentials.login());
                authenticator->setPassword(credentials.password());
        });

        QObject::connect(accessManager, &QNetworkAccessManager::proxyAuthenticationRequired,
            [this, parent](const QNetworkProxy& /*proxy*/, QAuthenticator* authenticator)
            {
                const auto user = parent->commonModule()->currentUrl().userName();
                const auto password = parent->commonModule()->currentUrl().password();
                authenticator->setUser(user);
                authenticator->setPassword(password);
            });

        installEventHandler(webView, QEvent::ContextMenu, webView,
            [webView](QObject* watched, QEvent* event)
            {
                NX_ASSERT(watched == webView && event->type() == QEvent::ContextMenu);
                const auto frame = webView->page()->mainFrame();
                if (!frame)
                    return;

                const auto menuEvent = static_cast<QContextMenuEvent*>(event);
                const auto hitTest = frame->hitTestContent(menuEvent->pos());

                webView->setContextMenuPolicy(hitTest.linkUrl().isEmpty()
                    ? Qt::DefaultContextMenu
                    : Qt::ActionsContextMenu); //< Special context menu for links.
            });

        installEventHandler(parent, QEvent::Show, parent,
            [this]()
            {
                if (!loadNeeded)
                    return;

                webWidget->load(lastRequest);
                loadNeeded = false;
            });
            return;
    }

    auto webView = webWidget->webEngineView();
    // Creating off-the-record profile.
    webEngineProfile.reset(new QWebEngineProfile(QString()));
    webEngineProfile->setHttpUserAgent(kUserAgentForCameraPage);
    webView->setPage(new WebEnginePage(webEngineProfile.data()));

    anchorWidgetToParent(webWidget);

    // TODO: Add special context menu for links

    QObject::connect(webView->page(), &QWebEnginePage::authenticationRequired,
        [this](const QUrl& requestUrl, QAuthenticator* authenticator)
        {
            QnMutexLocker lock(&mutex);
            if (lastRequest.url() != requestUrl)
                return;

            NX_ASSERT(credentials.login.hasValue() && credentials.password.hasValue());
            authenticator->setUser(credentials.login());
            authenticator->setPassword(credentials.password());
    });

    QObject::connect(webView->page(), &QWebEnginePage::proxyAuthenticationRequired,
        [this, parent](const QUrl&, QAuthenticator* authenticator, const QString&)
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

            webWidget->load(lastRequest.url());
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
        d->cookieJar->setCameraId(QnUuid(state.singleCameraProperties.id));
        d->credentials = state.credentials;

        if (!state.isSingleCamera() || state.singleCameraProperties.settingsUrlPath.isEmpty())
        {
            d->lastRequest = QNetworkRequest();
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

        if (auto webEngineView = d->webWidget->webEngineView())
        {
            QNetworkCookie cameraCookie(Qn::CAMERA_GUID_HEADER_NAME, QnUuid(state.singleCameraProperties.id).toByteArray());
            QUrl origin(targetUrl);
            origin.setUserName(QString());
            origin.setPassword(QString());
            origin.setPath(QString());
            webEngineView->page()->profile()->cookieStore()->setCookie(cameraCookie, origin);
        }

        auto gateway = nx::cloud::gateway::VmsGatewayEmbeddable::instance();

        // NOTE: Work-around to create ssl tunnel between gateway and server (not between the client
        // and server over proxy like in the normal case), because there is a bug in
        // QNetworkAccessManager (or with something related) which ignores proxy settings for some
        // requests from QWebPage when loading camera web page. Perhaps this is not a bug, but a
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

        if (d->webWidget->view())
            d->webWidget->view()->page()->networkAccessManager()->setProxy(gatewayProxy);
        else
            QNetworkProxy::setApplicationProxy(gatewayProxy);

        const QNetworkRequest request(targetUrl);
        if (d->lastRequest == request)
            return;

        NX_VERBOSE(this, "Loading state with request [%1] via proxy [%2:%3]",
            targetUrl, gatewayAddress.address.toString(), gatewayAddress.port);
        d->lastRequest = request;
        d->webWidget->reset();
    }

    const bool visible = isVisible();
    d->loadNeeded = !visible;
    if (visible)
        d->webWidget->load(d->lastRequest);
}

} // namespace nx::vms::client::desktop
