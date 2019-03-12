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

#include <common/common_module.h>
#include <ui/widgets/common/web_page.h>
#include <utils/common/event_processors.h>

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/web_widget.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/url/url_builder.h>

#include <nx/cloud/vms_gateway/vms_gateway_embeddable.h>

namespace nx::vms::client::desktop {

struct CameraWebPageWidget::Private
{
    Private(CameraWebPageWidget* parent);

    class WebPage;
    class CookieJar;

    WebWidget* const webWidget;
    const QScopedPointer<CookieJar> cookieJar;

    CameraSettingsDialogState::Credentials credentials;
    QNetworkRequest lastRequest;

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
        // This User-Agent is required for vista camera to use html/js page, not java applet.
        return lit("Mozilla/5.0 (Windows; U; Windows NT based; en-US)"
            " AppleWebKit/534.34 (KHTML, like Gecko)"
            "  QtWeb Internet Browser/3.8.5 http://www.QtWeb.net");
    }
};

CameraWebPageWidget::Private::Private(CameraWebPageWidget* parent):
    webWidget(new WebWidget(parent)),
    cookieJar(new CookieJar())
{
    auto webView = webWidget->view();
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
            return;
        }

        NX_ASSERT(d->credentials.login.hasValue() && d->credentials.password.hasValue());
        const auto currentServerUrl = commonModule()->currentUrl();

        const auto targetUrl = network::url::Builder()
            .setScheme(lit("http"))
            .setHost(currentServerUrl.host())
            .setPort(currentServerUrl.port())
            .setPath(state.singleCameraProperties.settingsUrlPath)
            .setUserName(d->credentials.login())
            .setPassword(d->credentials.password()).toUrl().toQUrl();
        NX_ASSERT(targetUrl.isValid());

        auto gateway = nx::cloud::gateway::VmsGatewayEmbeddable::instance();
        if (gateway->isSslEnabled())
        {
            gateway->enforceSslFor(
                nx::network::SocketAddress(currentServerUrl.host(), currentServerUrl.port()),
                currentServerUrl.scheme() == lit("https"));
        }

        const auto gatewayAddress = gateway->endpoint();
        const QNetworkProxy gatewayProxy(
            QNetworkProxy::HttpProxy,
            gatewayAddress.address.toString(), gatewayAddress.port,
            currentServerUrl.userName(), currentServerUrl.password());

        d->webWidget->view()->page()->networkAccessManager()->setProxy(gatewayProxy);

        const QNetworkRequest request(targetUrl);
        if (d->lastRequest == request)
            return;

        d->lastRequest = request;
        d->webWidget->reset();
    }

    d->webWidget->load(d->lastRequest);
}

} // namespace nx::vms::client::desktop
