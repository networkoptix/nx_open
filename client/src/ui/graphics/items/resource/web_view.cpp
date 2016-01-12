
#include "web_view.h"

#include <QtNetwork/QNetworkReply>
#include <QtWebKit/QWebHistory>

namespace
{
    enum ProgressValues
    {
        kProgressValueFailed = -1
        , kProgressValueStarted = 0
        , kProgressValueLoading = 85    // Up to 30% value of progress
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

QnWebView::QnWebView(const QUrl &url
    , QGraphicsItem *item)
    : QGraphicsWebView(item)
    , m_status(kPageLoadFailed)
    , m_canGoBack(false)
{
    setRenderHints(0);
    settings()->setAttribute(QWebSettings::TiledBackingStoreEnabled, true);
    settings()->setAttribute(QWebSettings::FrameFlatteningEnabled, true);

    connect(page()->networkAccessManager(), &QNetworkAccessManager::sslErrors,
        this, [](QNetworkReply* reply, const QList<QSslError> &){ reply->ignoreSslErrors();} );


    const auto progressHandler = [this](int progress)
    {
        setStatus(statusFromProgress(progress));
        qDebug() << "-- progress: " << progress;
    };

    const auto loadStartedHander = [this, progressHandler]()
    {
        connect(this, &QGraphicsWebView::loadProgress, this, progressHandler);
        setStatus(statusFromProgress(kProgressValueStarted));
        qDebug() << "****************** loadPage: " << this->url();
    };

    const auto loadFinishedHandler = [this](bool successful)
    {
        disconnect(this, &QGraphicsWebView::loadProgress, this, nullptr);
        setStatus(statusFromProgress(successful? kProgressValueLoaded : kProgressValueFailed));

        qDebug() << "----------------------- load finished: " << successful;
    };

    const auto onUrlChangedHandler = [this]()
    {
        setCanGoBack(history()->canGoBack());
    };

    connect(this, &QGraphicsWebView::loadStarted, this, loadStartedHander);
    connect(this, &QGraphicsWebView::loadFinished, this, loadFinishedHandler);
    connect(this, &QGraphicsWebView::loadProgress, this, progressHandler);
    connect(this, &QGraphicsWebView::urlChanged, this, onUrlChangedHandler);

    setUrl(url);
}

QnWebView::~QnWebView()
{
}

WebViewPageStatus QnWebView::status() const
{
    return m_status;
}

bool QnWebView::canGoBack() const
{
    return m_canGoBack;
}

void QnWebView::setCanGoBack(bool value)
{
    if (m_canGoBack == value)
        return;

    m_canGoBack = value;
    emit canGoBackChanged();
}

void QnWebView::setStatus(WebViewPageStatus value)
{
    if (m_status == value)
        return;

    m_status = value;
    emit statusChanged();
}

void QnWebView::updatePageLoadProgress(int progress)
{
    setStatus(statusFromProgress(progress));
}
