
#include "web_view.h"

#include <QtNetwork/QNetworkReply>

namespace
{
    enum ProgressValues
    {
        kProgressValueFailed = -1
        , kProgressValueStarted = 0
        , kProgressValueLoading = 30    // Up to 30% value of progress
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
{
    setRenderHints(0);
    settings()->setAttribute(QWebSettings::TiledBackingStoreEnabled, true);
    settings()->setAttribute(QWebSettings::FrameFlatteningEnabled, true);

    connect(page()->networkAccessManager(), &QNetworkAccessManager::sslErrors,
        this, [](QNetworkReply* reply, const QList<QSslError> &){ reply->ignoreSslErrors();} );

    loadPage(url);
}

QnWebView::~QnWebView()
{
}

WebViewPageStatus QnWebView::status() const
{
    return m_status;
}

void QnWebView::setStatus(WebViewPageStatus value)
{
    if (m_status == value)
        return;

    m_status = value;
    emit statusChanged();
}

void QnWebView::loadPage(const QUrl &url)
{
    stop();
    setUrl(QUrl()); // Clears previously set url

    setUrl(url);

    setStatus(statusFromProgress(kProgressValueStarted));

    qDebug() << "****************** loadPage: " << url;

    const auto loadFinishedHandler = [this](bool successful)
    {
        disconnect(this, &QGraphicsWebView::loadProgress, this, nullptr);
        setStatus(statusFromProgress(successful? kProgressValueLoaded : kProgressValueFailed));

        qDebug() << "----------------------- load finished: " << successful;
    };

    const auto progressHandler = [this](int progress)
    {
        setStatus(statusFromProgress(progress));
        qDebug() << "-- progress: " << progress;
    };

    connect(this, &QGraphicsWebView::loadFinished, this, loadFinishedHandler);
    connect(this, &QGraphicsWebView::loadProgress, this, progressHandler);
}

void QnWebView::reloadPage()
{
    const auto currentUrl = url();
    loadPage(currentUrl);
}

void QnWebView::updatePageLoadProgress(int progress)
{
    setStatus(statusFromProgress(progress));
}
