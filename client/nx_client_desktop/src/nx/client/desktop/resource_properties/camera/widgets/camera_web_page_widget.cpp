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
#include <QtWidgets/QLabel>
#include <QtWebKitWidgets/QWebFrame>
#include <QtWebKitWidgets/QWebPage>
#include <QtWebKitWidgets/QWebView>

#include <common/common_module.h>
#include <ui/style/webview_style.h>
#include <ui/widgets/common/web_page.h>
#include <utils/common/event_processors.h>

#include <nx/client/desktop/common/utils/widget_anchor.h>
#include <nx/client/desktop/common/widgets/busy_indicator.h>
#include <nx/network/http/custom_headers.h>

#include <vms_gateway_embeddable.h>

namespace nx {
namespace client {
namespace desktop {

struct CameraWebPageWidget::Private
{
    Private(CameraWebPageWidget* parent);
    void resetContent();

    class WebPage;
    class CookieJar;

    QWebView* const webView;
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
    webView(new QWebView(parent)),
    cookieJar(new CookieJar())
{
    NxUi::setupWebViewStyle(webView);
    webView->setPage(new WebPage(webView));
    new WidgetAnchor(webView);

    // Special actions list for context menu for links.
    webView->insertActions(nullptr, {
        webView->pageAction(QWebPage::OpenLink),
        webView->pageAction(QWebPage::Copy),
        webView->pageAction(QWebPage::CopyLinkToClipboard)});

    resetContent();

    static constexpr int kDotRadius = 8;
    auto busyIndicator = new BusyIndicatorWidget(parent);
    busyIndicator->setHidden(true);
    busyIndicator->dots()->setDotRadius(kDotRadius);
    busyIndicator->dots()->setDotSpacing(kDotRadius * 2);
    new WidgetAnchor(busyIndicator);

    auto errorLabel = new QLabel(parent);
    errorLabel->setText(lit("<h1>%1</h1>").arg(tr("Failed to load page")));
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setForegroundRole(QPalette::WindowText);
    new WidgetAnchor(errorLabel);

    QObject::connect(webView, &QWebView::loadStarted, busyIndicator, &QWidget::show);
    QObject::connect(webView, &QWebView::loadFinished, busyIndicator, &QWidget::hide);
    QObject::connect(webView, &QWebView::loadStarted, errorLabel, &QWidget::hide);
    QObject::connect(webView, &QWebView::loadFinished, errorLabel, &QWidget::setHidden);

    auto frame = webView->page()->mainFrame();
    frame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAsNeeded);
    frame->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAsNeeded);

    const auto accessManager = webView->page()->networkAccessManager();
    accessManager->setCookieJar(cookieJar.data());

    QObject::connect(accessManager, &QNetworkAccessManager::sslErrors,
        [](QNetworkReply* reply, const QList<QSslError>& /*errors*/) { reply->ignoreSslErrors(); });

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
        [this](QObject* watched, QEvent* event)
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

void CameraWebPageWidget::Private::resetContent()
{
    webView->triggerPageAction(QWebPage::Stop);
    webView->triggerPageAction(QWebPage::StopScheduledPageRefresh);
    webView->setContent({});
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

        if (state.singleCameraProperties.settingsUrlPath.isEmpty())
        {
            d->lastRequest = QNetworkRequest();
            d->resetContent();
            return;
        }

        // QUrl doesn't work if it isn't constructed from QString and uses credentials.
        // It stays invalid with error code 'AuthorityPresentAndPathIsRelative'
        //  --rvasilenko, Qt 5.2.1
        const auto currentServerUrl = commonModule()->currentUrl();
        QUrl targetUrl(lit("http://%1:%2/%3").arg(currentServerUrl.host(),
            QString::number(currentServerUrl.port()), state.singleCameraProperties.settingsUrlPath));

        NX_ASSERT(d->credentials.login.hasValue() && d->credentials.password.hasValue());
        targetUrl.setUserName(d->credentials.login());
        targetUrl.setPassword(d->credentials.password());

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

        d->webView->page()->networkAccessManager()->setProxy(gatewayProxy);

        const QNetworkRequest request(targetUrl);
        if (d->lastRequest == request)
            return;

        d->lastRequest = request;
        d->resetContent();
    }

    d->webView->load(d->lastRequest);
}

} // namespace desktop
} // namespace client
} // namespace nx
