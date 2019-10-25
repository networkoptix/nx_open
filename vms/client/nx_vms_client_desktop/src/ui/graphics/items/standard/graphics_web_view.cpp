#include "graphics_web_view.h"

#include <QtQuick/QQuickItem>

#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/style/webview_style.h>

namespace
{

enum ProgressValues
{
    kProgressValueFailed = -1,
    kProgressValueStarted = 0,
    kProgressValueLoading = 85, // Up to 85% value of progress
                                // we assume that page is in loading status
    kProgressValueLoaded = 100
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

} // namespace

namespace nx::vms::client::desktop {

const QUrl GraphicsWebEngineView::kQmlSourceUrl("qrc:/qml/Nx/Controls/WebEngineView.qml");

GraphicsWebEngineView::GraphicsWebEngineView(const QUrl &url, QGraphicsItem *parent):
    GraphicsQmlView(parent)
{
    setProperty(Qn::NoHandScrollOver, true);

    connect(this, &GraphicsQmlView::statusChanged, this,
        [this, url](QQuickWidget::Status status)
        {
            if (status != QQuickWidget::Ready)
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
            connect(webView, SIGNAL(loadProgressChanged()), this, SLOT(onLoadProgressChanged()));

            webView->setProperty("url", url);
        });

    const auto progressHandler = [this](int progress)
    {
        setStatus(statusFromProgress(progress));
    };

    const auto loadStartedHander = [this, progressHandler]()
    {
        connect(this, &GraphicsWebEngineView::loadProgress, this, progressHandler);
        setStatus(statusFromProgress(kProgressValueStarted));
    };

    const auto loadFinishedHandler = [this](bool successful)
    {
        disconnect(this, &GraphicsWebEngineView::loadProgress, this, nullptr);
        setStatus(statusFromProgress(successful ? kProgressValueLoaded : kProgressValueFailed));
    };

    connect(this, &GraphicsWebEngineView::loadStarted, this, loadStartedHander);
    connect(this, &GraphicsWebEngineView::loadFinished, this, loadFinishedHandler);
    connect(this, &GraphicsWebEngineView::loadProgress, this, progressHandler);

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

void GraphicsWebEngineView::onLoadProgressChanged()
{
    QQuickItem* webView = rootObject();
    if (!webView)
        return;
    emit loadProgress(webView->property("loadProgress").toInt());
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
    // Should be mapped to non-public QQuickWebEngineView::LoadStatus.
    enum WebEngineViewLoadStatus
    {
        LoadStartedStatus,
        LoadStoppedStatus,
        LoadSucceededStatus,
        LoadFailedStatus
    };

    switch (status)
    {
        case LoadStartedStatus:
            setStatus(kPageInitialLoadInProgress);
            emit loadStarted();
            break;
        case LoadStoppedStatus:
            setStatus(kPageLoaded);
            emit loadFinished(true);
            break;
        case LoadSucceededStatus:
            setStatus(kPageLoaded);
            emit loadFinished(true);
            break;
        case LoadFailedStatus:
        default:
            setStatus(kPageLoadFailed);
            emit loadFinished(false);
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
