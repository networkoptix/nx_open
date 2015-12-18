#include "web_resource_widget.h"

#include <QtNetwork/QNetworkReply>
#include <QtWebKitWidgets/QGraphicsWebView>

QnWebResourceWidget::QnWebResourceWidget( QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /*= NULL*/ )
    : base_type(context, item, parent)
{

    QGraphicsWebView *webView = new QGraphicsWebView(this);
    webView->setUrl(QUrl(lit("http://google.ru")));
    webView->setRenderHints(0);
    webView->settings()->setAttribute(QWebSettings::TiledBackingStoreEnabled, true); 
    webView->settings()->setAttribute(QWebSettings::FrameFlatteningEnabled, true); 

    connect(webView->page()->networkAccessManager(), &QNetworkAccessManager::sslErrors,
        this, [](QNetworkReply* reply, const QList<QSslError> &){ reply->ignoreSslErrors();} );
//     connect(webView->page()->networkAccessManager(), &QNetworkAccessManager::authenticationRequired,
//         this, &::at_authenticationRequired, Qt::DirectConnection );
//     connect(webView->page()->networkAccessManager(), &QNetworkAccessManager::proxyAuthenticationRequired,
//         this, &::at_proxyAuthenticationRequired, Qt::DirectConnection);

    addOverlayWidget(webView, detail::OverlayedBase::Visible, false, true, BaseLayer);

    setOption(QnResourceWidget::WindowRotationForbidden, true);

    updateTitleText();
    updateInfoText();
    updateDetailsText();
    updateButtonsVisibility();
}

QnWebResourceWidget::~QnWebResourceWidget()
{

}

QString QnWebResourceWidget::calculateTitleText() const
{
    return lit("www.yandex.ru");
}

Qn::ResourceStatusOverlay QnWebResourceWidget::calculateStatusOverlay() const
{
    return Qn::EmptyOverlay;
}

Qn::RenderStatus QnWebResourceWidget::paintChannelBackground( QPainter *painter, int channel, const QRectF &channelRect, const QRectF &paintRect )
{
    Q_UNUSED(painter)
    Q_UNUSED(channel)
    Q_UNUSED(channelRect)

    if (!paintRect.isValid())
        return Qn::NothingRendered;

    return Qn::NewFrameRendered;
}
