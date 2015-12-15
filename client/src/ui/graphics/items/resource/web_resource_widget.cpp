#include "web_resource_widget.h"

#include <QtWebKitWidgets/QGraphicsWebView>

QnWebResourceWidget::QnWebResourceWidget( QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /*= NULL*/ )
    : base_type(context, item, parent)
{

    QGraphicsWebView *webView = new QGraphicsWebView(this);
    webView->setUrl(QUrl(lit("www.yandex.ru")));
    addOverlayWidget(webView, detail::OverlayedBase::Visible, false, true, BaseLayer);

    setOption(QnResourceWidget::WindowRotationForbidden, true);
    setInfoVisible(true, false);
}

QnWebResourceWidget::~QnWebResourceWidget()
{

}

QnResourceWidget::Buttons QnWebResourceWidget::calculateButtonsVisibility() const
{
    return 0;
}

QString QnWebResourceWidget::calculateTitleText() const
{
    return lit("www.yandex.ru");
}

Qn::ResourceStatusOverlay QnWebResourceWidget::calculateStatusOverlay() const
{
    return Qn::EmptyOverlay;
}
