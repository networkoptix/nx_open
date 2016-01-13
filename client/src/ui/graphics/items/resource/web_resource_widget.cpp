#include "web_resource_widget.h"

#include <core/resource/resource.h>

#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/resource/web_view.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/overlays/buttons_overlay.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>

namespace
{
    class SelectByClickOverlay : public QGraphicsWidget
    {
    public:
        SelectByClickOverlay(QnWebResourceWidget *parent
            , QnWorkbenchItem *item
            , QnWorkbench *workbench
            , QMenu *menu);

    private:
        void mousePressEvent(QGraphicsSceneMouseEvent *event);

        void contextMenuEvent(QGraphicsSceneContextMenuEvent * event);

    private:
        QnWebResourceWidget * const m_widget;
        QnWorkbenchItem * const m_item;
        QnWorkbench * const m_workbench;
        QMenu * const m_menu;
    };

    SelectByClickOverlay::SelectByClickOverlay(QnWebResourceWidget *parent
        , QnWorkbenchItem *item
        , QnWorkbench *workbench
        , QMenu *menu)
        : QGraphicsWidget(parent)
        , m_widget(parent)
        , m_item(item)
        , m_workbench(workbench)
        , m_menu(menu)
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


    void SelectByClickOverlay::contextMenuEvent(QGraphicsSceneContextMenuEvent * event)
    {
        /*  // Just blocks context menus anyway now
        if (m_menu)
            m_menu->exec(event->screenPos());
        else
            event->ignore();
        */
    }

}

QnWebResourceWidget::QnWebResourceWidget( QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /*= NULL*/ )
    : base_type(context, item, parent)
    , m_webView(new QnWebView(resource()->getUrl(), this))
{
    setOption(AlwaysShowName, true);

    const auto iconButton = buttonsOverlay()->leftButtonsBar()->button(Qn::kRecordingStatusIconButton);
    const auto contentMargins = detail::OverlayMargins(0, iconButton->preferredHeight());
    const auto webParams = detail::OverlayParams(Visible
        , false, true, BaseLayer, contentMargins);
    addOverlayWidget(m_webView, webParams);

    setOption(QnResourceWidget::WindowRotationForbidden, true);
    updateTitleText();
    updateInfoText();
    updateDetailsText();

    addOverlayWidget(new SelectByClickOverlay(this, item, context->workbench(), nullptr)
        , detail::OverlayParams(Visible, true, true, TopControlsLayer, contentMargins));

    const auto updateStatusesHandler = [this]()
    {
        const auto status = m_webView->status();
        const auto resourceStatus = (status == kPageLoadFailed ? Qn::Offline : Qn::Online);
        resource()->setStatus(resourceStatus);

        updateStatusOverlay();
    };

    connect(m_webView, &QnWebView::statusChanged, this, updateStatusesHandler);

    setupOverlays();
    updateButtonsVisibility();
}

QnWebResourceWidget::~QnWebResourceWidget()
{
    /* statusChanged will be emitted during the destruction process. */
    disconnect(m_webView, nullptr, this, nullptr);
}

void QnWebResourceWidget::setupOverlays()
{
    {
        // Left buttons bar setup
        auto buttonsBar = buttonsOverlay()->leftButtonsBar();

        auto backButton = new QnImageButtonWidget(this);
        backButton->setIcon(qnSkin->icon("item/back.png"));
        connect(backButton, &QnImageButtonWidget::clicked, m_webView, &QnWebView::back);
        buttonsBar->addButton(Qn::kBackButton, backButton);
        buttonsBar->setButtonsEnabled(Qn::kBackButton, false);

        connect(m_webView, &QnWebView::canGoBackChanged, this, [this, buttonsBar, backButton]()
        {
            buttonsBar->setButtonsEnabled(Qn::kBackButton, m_webView->canGoBack());
        });

        auto reloadButton = new QnImageButtonWidget(this);
        reloadButton->setIcon(qnSkin->icon("item/refresh.png"));
        connect(reloadButton, &QnImageButtonWidget::clicked, this, [this]()
        {
            // We can't use QnWebView::reload because if it was an
            // error previously, reload does not work

            m_webView->setPageUrl(m_webView->url());
        });
        buttonsBar->addButton(Qn::kReloadPageButton, reloadButton);
    }

    {
        // Right buttons bar setup
        QnImageButtonWidget *fullscreenButton= new QnImageButtonWidget();
        fullscreenButton->setIcon(qnSkin->icon("item/fullscreen.png"));
        fullscreenButton->setProperty(Qn::NoBlockMotionSelection, true);
        fullscreenButton->setToolTip(tr("Fullscreen mode"));
        connect(fullscreenButton, &QnImageButtonWidget::clicked, this, [this]()
        {
            // Toggles fullscreen item mode
            const auto newFullscreenItem = (options().testFlag(FullScreenMode) ? nullptr : item());
            workbench()->setItem(Qn::ZoomedRole, newFullscreenItem);
        });
        buttonsOverlay()->rightButtonsBar()->addButton(Qn::kFullscreenButton, fullscreenButton);
    }
}

Qn::ResourceStatusOverlay QnWebResourceWidget::calculateStatusOverlay() const
{
    switch(m_webView->status())
    {
    case kPageLoadFailed:
        return Qn::OfflineOverlay;
    case kPageInitialLoadInProgress:
        return Qn::LoadingOverlay;
    //case kPageLoading:
    //case kPageLoaded:
    default:
        return Qn::EmptyOverlay;
    }
}

void QnWebResourceWidget::optionsChangedNotify(Options changedFlags)
{
    const bool fullscreenModeChanged = changedFlags.testFlag(FullScreenMode);
    if (fullscreenModeChanged)
    {
        updateHud(true);

        const bool fullscreenMode = options().testFlag(FullScreenMode);
        const auto newIcon = fullscreenMode
            ? qnSkin->icon("item/exit_fullscreen.png")
            : qnSkin->icon("item/fullscreen.png");

        buttonsOverlay()->rightButtonsBar()->button(Qn::kFullscreenButton)->setIcon(newIcon);
    }

    return base_type::optionsChangedNotify(changedFlags);
}

int QnWebResourceWidget::calculateButtonsVisibility() const
{
    return (base_type::calculateButtonsVisibility()
        | Qn::kFullscreenButton | Qn::kBackButton | Qn::kReloadPageButton);
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
