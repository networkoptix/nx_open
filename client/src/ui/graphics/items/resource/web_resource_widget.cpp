#include "web_resource_widget.h"

#include <core/resource/resource.h>

#include <QtNetwork/QNetworkReply>

#ifdef GDM_TODO
#include <QtWebKitWidgets/QGraphicsWebView>
#endif

QnWebResourceWidget::QnWebResourceWidget( QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /*= NULL*/ )
    : base_type(context, item, parent)
{
#ifdef GDM_TODO
    QGraphicsWebView *webView = new QGraphicsWebView(this);
    webView->setUrl(QUrl(resource()->getUrl()));
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
#endif
    updateTitleText();
    updateInfoText();
    updateDetailsText();
    updateButtonsVisibility();
}

QnWebResourceWidget::~QnWebResourceWidget()
{

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

QString QnWebResourceWidget::calculateDetailsText() const
{
    return resource()->getUrl();
}
