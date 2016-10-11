#include "web_view.h"

#include <QtNetwork/QNetworkReply>
#include <QtWebKit/QWebHistory>

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

QnWebView::QnWebView(const QUrl &url
    , QGraphicsItem *item)
    : QGraphicsWebView(item)
    , m_status(kPageLoadFailed)
    , m_canGoBack(false)
{
    setRenderHints(0);
    settings()->setAttribute(QWebSettings::TiledBackingStoreEnabled, true);
    settings()->setAttribute(QWebSettings::FrameFlatteningEnabled, true);

    setupStyle();
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

    connect(this, &QGraphicsWebView::linkClicked, this, &QnWebView::setPageUrl);
    connect(this, &QGraphicsWebView::loadStarted, this, loadStartedHander);
    connect(this, &QGraphicsWebView::loadFinished, this, loadFinishedHandler);
    connect(this, &QGraphicsWebView::loadProgress, this, progressHandler);
    connect(this, &QGraphicsWebView::urlChanged, this, onUrlChangedHandler);

    page()->setLinkDelegationPolicy(QWebPage::DelegateExternalLinks);
    setPageUrl(url);
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

void QnWebView::setPageUrl(const QUrl &newUrl)
{
    stop();

    QWebSettings::clearMemoryCaches();
    setUrl(newUrl);
}


void QnWebView::setupStyle()
{
    //TODO: #GDM 90% of this is not used really.

    QStyle* style = QStyleFactory().create(lit("fusion"));
    setStyle(style);

    QPalette palette = this->palette();

    // Outline around the menu
    palette.setColor(QPalette::Window, Qt::gray);
    palette.setColor(QPalette::WindowText, Qt::black);

    palette.setColor(QPalette::BrightText, Qt::gray);
    palette.setColor(QPalette::BrightText, Qt::black);

    // combo button
    palette.setColor(QPalette::Button, Qt::gray);
    palette.setColor(QPalette::ButtonText, Qt::black);

    // combo menu
    palette.setColor(QPalette::Base, Qt::gray);
    palette.setColor(QPalette::Text, Qt::black);

    // tool tips
    palette.setColor(QPalette::ToolTipBase, Qt::gray);
    palette.setColor(QPalette::ToolTipText, Qt::black);

    palette.setColor(QPalette::NoRole, Qt::gray);
    palette.setColor(QPalette::AlternateBase, Qt::gray);

    palette.setColor(QPalette::Link, Qt::black);
    palette.setColor(QPalette::LinkVisited, Qt::black);

    // highlight button & menu
    palette.setColor(QPalette::Highlight, Qt::gray);
    palette.setColor(QPalette::HighlightedText, Qt::black);

    // to customize the disabled color
    palette.setColor(QPalette::Disabled, QPalette::Button, Qt::gray);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::black);

    setPalette(palette);
    setAutoFillBackground(true);
}

void QnWebView::setStatus(WebViewPageStatus value)
{
    if (m_status == value)
        return;

    m_status = value;
    emit statusChanged();
}
