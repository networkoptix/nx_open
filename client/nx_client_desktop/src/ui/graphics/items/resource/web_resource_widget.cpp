#include "web_resource_widget.h"

#include <core/resource/resource.h>

#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/standard/graphics_web_view.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/overlays/resource_title_item.h>
#include <ui/help/help_topics.h>

QnWebResourceWidget::QnWebResourceWidget( QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /*= NULL*/ )
    : base_type(context, item, parent)
    , m_webView(new QnGraphicsWebView(resource()->getUrl(), this))
{
    setOption(AlwaysShowName, true);

    m_webView->installEventFilter(this);
    const auto iconButton = titleBar()->leftButtonsBar()->button(Qn::RecordingStatusIconButton);
    const auto contentMargins = QMarginsF(0, iconButton->preferredHeight(), 0, 0);
    const auto webParams = detail::OverlayParams(Visible
        , false, true, BaseLayer, contentMargins);
    addOverlayWidget(m_webView, webParams);

    setOption(QnResourceWidget::WindowRotationForbidden, true);
    updateTitleText();
    updateInfoText();
    updateDetailsText();

    const auto updateStatusesHandler = [this]()
        {
            const auto status = m_webView->status();
            const auto resourceStatus = (status == kPageLoadFailed ? Qn::Offline : Qn::Online);
            resource()->setStatus(resourceStatus);

            updateStatusOverlay(true);
        };

    connect(m_webView, &QnGraphicsWebView::statusChanged, this, updateStatusesHandler);

    connect(resource(), &QnResource::urlChanged, this,
        [this]()
        {
            m_webView->setUrl(resource()->getUrl());
        });

    setupOverlays();
    updateButtonsVisibility();
}

QnWebResourceWidget::~QnWebResourceWidget()
{
    /* statusChanged will be emitted during the destruction process. */
    disconnect(m_webView, nullptr, this, nullptr);
}

int QnWebResourceWidget::helpTopicAt(const QPointF& /*pos*/) const
{
    return Qn::MainWindow_WebPageItem_Help;
}

void QnWebResourceWidget::setupOverlays()
{
    {
        // Left buttons bar setup
        auto buttonsBar = titleBar()->leftButtonsBar();

        auto backButton = createStatisticAwareButton(lit("web_widget_back"));
        backButton->setIcon(qnSkin->icon("item/back.png"));
        connect(backButton, &QnImageButtonWidget::clicked, m_webView, &QnGraphicsWebView::back);
        buttonsBar->addButton(Qn::BackButton, backButton);
        buttonsBar->setButtonsEnabled(Qn::BackButton, false);

        connect(m_webView, &QnGraphicsWebView::canGoBackChanged, this, [this, buttonsBar, backButton]()
        {
            buttonsBar->setButtonsEnabled(Qn::BackButton, m_webView->canGoBack());
        });

        // Should force HUD to update details text with new URL
        connect(m_webView, &QnGraphicsWebView::loadStarted, this, [this]()
        {
            this->updateDetailsText();
        });

        auto reloadButton = createStatisticAwareButton(lit("web_widget_reload"));
        reloadButton->setIcon(qnSkin->icon("item/refresh.png"));
        connect(reloadButton, &QnImageButtonWidget::clicked, this, [this]()
        {
            // We can't use QnGraphicsWebView::reload because if it was an
            // error previously, reload does not work

            m_webView->setPageUrl(m_webView->url());
        });
        buttonsBar->addButton(Qn::ReloadPageButton, reloadButton);
    }

    {
        // Right buttons bar setup
        auto fullscreenButton= createStatisticAwareButton(lit("web_widget_fullscreen"));
        fullscreenButton->setIcon(qnSkin->icon("item/fullscreen.png"));
        fullscreenButton->setToolTip(tr("Fullscreen mode"));
        connect(fullscreenButton, &QnImageButtonWidget::clicked, this, [this]()
        {
            // Toggles fullscreen item mode
            const auto newFullscreenItem = (options().testFlag(FullScreenMode) ? nullptr : item());
            workbench()->setItem(Qn::ZoomedRole, newFullscreenItem);
        });
        titleBar()->rightButtonsBar()->addButton(Qn::FullscreenButton, fullscreenButton);
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

        titleBar()->rightButtonsBar()->button(Qn::FullscreenButton)->setIcon(newIcon);
    }

    return base_type::optionsChangedNotify(changedFlags);
}

bool QnWebResourceWidget::eventFilter(QObject *object
    , QEvent *event)
{
    if (object == m_webView )
    {
        if (event->type() == QEvent::GraphicsSceneMousePress)
            mousePressEvent(static_cast<QGraphicsSceneMouseEvent *>(event));
        else if (event->type() == QEvent::GraphicsSceneContextMenu)
            return true;
    }

    return base_type::eventFilter(object, event);
}

int QnWebResourceWidget::calculateButtonsVisibility() const
{
    return (base_type::calculateButtonsVisibility()
        | Qn::FullscreenButton | Qn::BackButton | Qn::ReloadPageButton);
}

Qn::RenderStatus QnWebResourceWidget::paintChannelBackground(QPainter* painter, int channel,
    const QRectF& channelRect, const QRectF& paintRect)
{
    if (!paintRect.isValid())
        return Qn::NothingRendered;

    return base_type::paintChannelBackground(painter, channel, channelRect, paintRect);
}

QString QnWebResourceWidget::calculateDetailsText() const
{
    NX_ASSERT(m_webView != nullptr, Q_FUNC_INFO, "webview widget is null");
    const int kMaxUrlDisplayLength = 96;
    // Truncating URL if it is too long to display properly
    // I could strip query part from URL, but some sites, like youtibe will be stripped too much
    QString details = m_webView->url().toString();
    if (details.length() > kMaxUrlDisplayLength)
    {
        details = details.left(kMaxUrlDisplayLength) + QString::fromUtf8("...");
    }
    return details;
}
