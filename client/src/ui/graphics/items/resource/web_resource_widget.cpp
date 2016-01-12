#include "web_resource_widget.h"

#include <core/resource/resource.h>

#include <QtNetwork/QNetworkReply>
#include <QtWebKitWidgets/QGraphicsWebView>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/graphics/items/generic/image_button_widget.h>

namespace
{
    class SelectByClickOverlay : public QGraphicsWidget
    {
    public:
        SelectByClickOverlay(QnWebResourceWidget *parent
            , QnWorkbenchItem *item
            , QnWorkbench *workbench);

    private:
        void mousePressEvent(QGraphicsSceneMouseEvent *event);

    private:
        QnWebResourceWidget * const m_widget;
        QnWorkbenchItem * const m_item;
        QnWorkbench * const m_workbench;
    };

    SelectByClickOverlay::SelectByClickOverlay(QnWebResourceWidget *parent
        , QnWorkbenchItem *item
        , QnWorkbench *workbench)
        : QGraphicsWidget(parent)
        , m_widget(parent)
        , m_item(item)
        , m_workbench(workbench)
    {}

    void SelectByClickOverlay::mousePressEvent(QGraphicsSceneMouseEvent *event)
    {
        if((event->button() == Qt::LeftButton) && (m_widget->flags() & ItemIsSelectable))
        {
            m_workbench->setItem(Qn::RaisedRole, nullptr);

            const bool multiSelect = ((event->modifiers() & Qt::ControlModifier) != 0);
            if (multiSelect)
            {
                if (!m_widget->isSelected())
                    m_widget->selectThisWidget(false);
                else
                    m_widget->setSelected(false);
            }
            else
                m_widget->selectThisWidget(true);

        }
        event->ignore();
    }

}

QnWebResourceWidget::QnWebResourceWidget( QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /*= NULL*/ )
    : base_type(context, item, parent)
{
    setOption(AlwaysShowName, true);

//    QStyle* webStyle = QStyleFactory::create(lit("Fusion"));

    QGraphicsWebView *webView = new QGraphicsWebView(this);
//    webView->setStyle(webStyle);
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

    const auto contentMargins = detail::OverlayMargins(0, iconButton()->preferredHeight());
    const auto webParams = detail::OverlayParams(Visible
        , false, true, BaseLayer, contentMargins);
    addOverlayWidget(webView, webParams);

    setOption(QnResourceWidget::WindowRotationForbidden, true);
    updateTitleText();
    updateInfoText();
    updateDetailsText();
    updateButtonsVisibility();

    addOverlayWidget(new SelectByClickOverlay(this, item, context->workbench())
        , detail::OverlayParams(Visible, true, true, TopControlsLayer, contentMargins));
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
