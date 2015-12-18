#include "web_resource_widget.h"

//#include <QtWebKitWidgets/QGraphicsWebView>

QnWebResourceWidget::QnWebResourceWidget( QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /*= NULL*/ )
    : base_type(context, item, parent)
{

  /*  QGraphicsWebView *webView = new QGraphicsWebView(this);
    webView->setUrl(QUrl(lit("http://bash.im")));
    addOverlayWidget(webView, detail::OverlayedBase::Visible, false, true, BaseLayer);
*/
    setOption(QnResourceWidget::WindowRotationForbidden, true);

    updateTitleText();
    updateInfoText();
    updateDetailsText();
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
