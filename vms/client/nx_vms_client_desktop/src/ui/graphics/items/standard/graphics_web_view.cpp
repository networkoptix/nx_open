#include "graphics_web_view.h"

#include <QtNetwork/QNetworkReply>
#include <QtWidgets/QGraphicsOpacityEffect>
#include <QtWebKit/QWebHistory>
#include <QQuickItem>

#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/style/webview_style.h>
#include <ui/widgets/common/web_page.h>

namespace
{
    enum ProgressValues
    {
        kProgressValueFailed = -1
        , kProgressValueStarted = 0
        , kProgressValueLoading = 85    // Up to 85% value of progress
                                        // we assume that page is in loading status
        , kProgressValueLoaded = 100
    };

    WebViewPageStatus statusFromProgress(int progress)
    {
        switch (progress)
        {
        case kProgressValueFailed:
            return kPageLoadFailed;
        case kProgressValueLoaded:
            return kPageLoaded;
        default:
            return (progress < kProgressValueLoading ? kPageInitialLoadInProgress : kPageLoading);
        }
    }
}

namespace nx::vms::client::desktop {

const QUrl GraphicsWebEngineView::kQmlSourceUrl("qrc:/qml/Nx/Controls/NxWebEngineView.qml");

GraphicsWebEngineView::GraphicsWebEngineView(const QUrl &url, QGraphicsItem *parent)
    : GraphicsQmlView(parent)
    , m_status(kPageLoadFailed)
    , m_canGoBack(false)
{
    setProperty(Qn::NoHandScrollOver, true);

    connect(this, &GraphicsQmlView::statusChanged, this,
        [this, url](QQmlComponent::Status status)
        {
            if (status != QQmlComponent::Ready)
                return;

            QQuickItem* webView = rootObject();
            if (!webView)
                return;

            if (m_rootReadyCallback)
            {
                m_rootReadyCallback();
                m_rootReadyCallback = nullptr;
            }

            connect(webView, SIGNAL(urlChanged()), this, SLOT(viewUrlChanged()));
            connect(webView, SIGNAL(loadingStatusChanged(int)), this, SLOT(setViewStatus(int)));

            webView->setProperty("url", url);
        });

    setSource(kQmlSourceUrl);
}

void GraphicsWebEngineView::whenRootReady(RootReadyCallback callback)
{
    if (rootObject())
    {
        m_rootReadyCallback = nullptr;
        callback();
        return;
    }
    m_rootReadyCallback = callback;
}

GraphicsWebEngineView::~GraphicsWebEngineView()
{
}

void GraphicsWebEngineView::registerObject(QQuickItem* webView, const QString& name, QObject* object, bool enablePromises)
{
    if (!webView)
        return;
    QObject* webChannel = webView->findChild<QObject*>("nxWebChannel");
    if (!webChannel)
        return;
    QVariantMap objects;
    objects.insert(name,  QVariant::fromValue<QObject*>(object));
    QMetaObject::invokeMethod(webChannel, "registerObjects", Q_ARG(QVariantMap, objects));
    webView->setProperty("enableInjections", true);
    webView->setProperty("enablePromises", enablePromises);
}

void GraphicsWebEngineView::addToJavaScriptWindowObject(const QString& name, QObject* object)
{
    registerObject(rootObject(), name, object);
}

void GraphicsWebEngineView::setViewStatus(int status)
{
    switch(status)
    {
        case 0: // WebEngineView.LoadStartedStatus
            setStatus(kPageLoading);
            emit loadStarted();
            break;
        case 1: // WebEngineView.LoadStoppedStatus
            setStatus(kPageLoaded);
            break;
        case 2: // WebEngineView.LoadSucceededStatus
            setStatus(kPageLoaded);
            break;
        case 3: // WebEngineView.LoadFailedStatus
        default:
            setStatus(kPageLoadFailed);
    }
}

void GraphicsWebEngineView::viewUrlChanged()
{
    QQuickItem* webView = rootObject();
    if (!webView)
        return;
    setCanGoBack(webView->property("canGoBack").toBool());
}

WebViewPageStatus GraphicsWebEngineView::status() const
{
    return m_status;
}

bool GraphicsWebEngineView::canGoBack() const
{
    return m_canGoBack;
}

void GraphicsWebEngineView::setCanGoBack(bool value)
{
    if (m_canGoBack == value)
        return;

    m_canGoBack = value;
    emit canGoBackChanged();
}

void GraphicsWebEngineView::setPageUrl(const QUrl &newUrl)
{
    QQuickItem* webView = rootObject();
    if (!webView)
        return;
    QMetaObject::invokeMethod(webView, "stop");

    // FIXME: There is no QWebSettings::clearMemoryCaches() for QWebEngine
    setUrl(newUrl);
}

void GraphicsWebEngineView::setUrl(const QUrl &url)
{
    QQuickItem* webView = rootObject();
    if (!webView)
        return;
    webView->setProperty("url", url);
}

QUrl GraphicsWebEngineView::url() const
{
    QQuickItem* webView = rootObject();
    if (!webView)
        return QUrl();
    return webView->property("url").toUrl();
}

void GraphicsWebEngineView::back()
{
    QQuickItem* webView = rootObject();
    if (!webView)
        return;
    QMetaObject::invokeMethod(webView, "goBack");
}

void GraphicsWebEngineView::setStatus(WebViewPageStatus value)
{
    if (m_status == value)
        return;

    m_status = value;
    emit statusChanged();
}

} // namespace nx::vms::client::desktop

QnGraphicsWebView::QnGraphicsWebView(const QUrl &url
    , QGraphicsItem *item)
    : QGraphicsWebView(item)
    , m_status(kPageLoadFailed)
    , m_canGoBack(false)
{
    setRenderHints(0);
    setAcceptDrops(false);

    // Forcefully create additional non-screen buffer to make transparency composition work in the
    // webkit engine over the QOpenGLWidget.
    auto opacityEffect = new QGraphicsOpacityEffect(this);
    opacityEffect->setOpacity(0.99); //< 1.0 can be optimized and ignored.
    setGraphicsEffect(opacityEffect);

    settings()->setAttribute(QWebSettings::PluginsEnabled, true);
    settings()->setAttribute(QWebSettings::TiledBackingStoreEnabled, true);
    settings()->setAttribute(QWebSettings::FrameFlatteningEnabled, true);

    NxUi::setupWebViewStyle(this);
    setPage(new QnWebPage(this));

    connect(page()->networkAccessManager(), &QNetworkAccessManager::sslErrors,
        this, [](QNetworkReply* reply, const QList<QSslError> &){ reply->ignoreSslErrors();} );


    const auto progressHandler = [this](int progress)
    {
        setStatus(statusFromProgress(progress));
    };

    const auto loadStartedHander = [this, progressHandler]()
    {
        connect(this, &QGraphicsWebView::loadProgress, this, progressHandler);
        setStatus(statusFromProgress(kProgressValueStarted));
    };

    const auto loadFinishedHandler = [this](bool successful)
    {
        disconnect(this, &QGraphicsWebView::loadProgress, this, nullptr);
        setStatus(statusFromProgress(successful? kProgressValueLoaded : kProgressValueFailed));
    };

    const auto onUrlChangedHandler = [this]()
    {
        setCanGoBack(history()->canGoBack());
    };

    connect(this, &QGraphicsWebView::linkClicked, this, &QnGraphicsWebView::setPageUrl);
    connect(this, &QGraphicsWebView::loadStarted, this, loadStartedHander);
    connect(this, &QGraphicsWebView::loadFinished, this, loadFinishedHandler);
    connect(this, &QGraphicsWebView::loadProgress, this, progressHandler);
    connect(this, &QGraphicsWebView::urlChanged, this, onUrlChangedHandler);

    page()->setLinkDelegationPolicy(QWebPage::DelegateExternalLinks);
    setPageUrl(url);
}

QnGraphicsWebView::~QnGraphicsWebView()
{
}

WebViewPageStatus QnGraphicsWebView::status() const
{
    return m_status;
}

bool QnGraphicsWebView::canGoBack() const
{
    return m_canGoBack;
}

void QnGraphicsWebView::setCanGoBack(bool value)
{
    if (m_canGoBack == value)
        return;

    m_canGoBack = value;
    emit canGoBackChanged();
}

void QnGraphicsWebView::setPageUrl(const QUrl &newUrl)
{
    stop();

    QWebSettings::clearMemoryCaches();
    setUrl(newUrl);
}

void QnGraphicsWebView::setStatus(WebViewPageStatus value)
{
    if (m_status == value)
        return;

    m_status = value;
    emit statusChanged();
}
