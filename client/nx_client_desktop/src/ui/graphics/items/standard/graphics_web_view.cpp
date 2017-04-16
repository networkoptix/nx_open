#include "graphics_web_view.h"

#include <QtNetwork/QNetworkReply>
#include <QtWebKit/QWebHistory>

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

QnGraphicsWebView::QnGraphicsWebView(const QUrl &url
    , QGraphicsItem *item)
    : QGraphicsWebView(item)
    , m_status(kPageLoadFailed)
    , m_canGoBack(false)
{
    setRenderHints(0);
    setAcceptDrops(false);

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

    const auto linkClickedHandler = [this](const QUrl &linkUrl)
    {
        setPageUrl(linkUrl);
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
