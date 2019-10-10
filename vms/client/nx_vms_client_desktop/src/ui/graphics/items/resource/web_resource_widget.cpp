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
#include <nx/vms/client/desktop/ini.h>

using namespace nx::vms::client::desktop;

QnWebResourceWidget::QnWebResourceWidget( QnWorkbenchContext *context, QnWorkbenchItem *item, QGraphicsItem *parent /*= NULL*/ )
    : base_type(context, item, parent)
    , m_webView(nullptr)
    , m_webEngineView(nullptr)
{
    if (ini().useWebEngine)
        m_webEngineView = new GraphicsWebEngineView(resource()->getUrl());
    else
        m_webView = new QnGraphicsWebView(resource()->getUrl());

    setOption(AlwaysShowName, true);

    if (m_webView)
        m_webView->installEventFilter(this);
    const auto iconButton = titleBar()->leftButtonsBar()->button(Qn::RecordingStatusIconButton);
    const auto contentMargins = QMarginsF(0, iconButton->preferredHeight(), 0, 0);
    const auto webParams = detail::OverlayParams(Visible
        , false, true, BaseLayer, contentMargins);
    if (m_webView)
        addOverlayWidget(m_webView, webParams);
    else
        addOverlayWidget(m_webEngineView, webParams);

    setOption(QnResourceWidget::WindowRotationForbidden, true);
    updateTitleText();
    updateInfoText();
    updateDetailsText();

    const auto updateStatusesHandler = [this]()
        {
            const auto status = m_webView ? m_webView->status() : m_webEngineView->status();
            const auto resourceStatus = (status == kPageLoadFailed ? Qn::Offline : Qn::Online);
            resource()->setStatus(resourceStatus);

            updateStatusOverlay(true);
        };

    if (m_webView)
        connect(m_webView, &QnGraphicsWebView::statusChanged, this, updateStatusesHandler);
    else
        connect(m_webEngineView, &GraphicsWebEngineView::statusChanged, this, updateStatusesHandler);

    connect(resource(), &QnResource::urlChanged, this,
        [this]()
        {
            if (m_webView)
                m_webView->setUrl(resource()->getUrl());
            else
                m_webEngineView->setUrl(resource()->getUrl());
        });

    setupOverlays();
    updateButtonsVisibility();
}

QnWebResourceWidget::~QnWebResourceWidget()
{
    /* statusChanged will be emitted during the destruction process. */
    if (m_webView)
        disconnect(m_webView, nullptr, this, nullptr);
    else
        disconnect(m_webEngineView, nullptr, this, nullptr);
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
        if (m_webView)
            connect(backButton, &QnImageButtonWidget::clicked, m_webView, &QnGraphicsWebView::back);
        else
            connect(backButton, &QnImageButtonWidget::clicked, m_webEngineView, &GraphicsWebEngineView::back);
        buttonsBar->addButton(Qn::BackButton, backButton);
        buttonsBar->setButtonsEnabled(Qn::BackButton, false);

        if (m_webView)
        {
            connect(m_webView, &QnGraphicsWebView::canGoBackChanged, this,
                [this, buttonsBar, backButton]()
                {
                    buttonsBar->setButtonsEnabled(Qn::BackButton, m_webView->canGoBack());
                });

            // Should force HUD to update details text with new URL
            connect(m_webView, &QnGraphicsWebView::loadStarted, this,
                [this]()
                {
                    this->updateDetailsText();
                });
        }
        else
        {
            connect(m_webEngineView, &GraphicsWebEngineView::canGoBackChanged, this,
                [this, buttonsBar, backButton]()
                {
                    buttonsBar->setButtonsEnabled(Qn::BackButton, m_webEngineView->canGoBack());
                });

            // Should force HUD to update details text with new URL
            connect(m_webEngineView, &GraphicsWebEngineView::loadStarted, this,
                [this]()
                {
                    this->updateDetailsText();
                });
        }

        auto reloadButton = createStatisticAwareButton(lit("web_widget_reload"));
        reloadButton->setIcon(qnSkin->icon("item/refresh.png"));
        connect(reloadButton, &QnImageButtonWidget::clicked, this,
            [this]()
            {
                // We can't use QnGraphicsWebView::reload because if it was an
                // error previously, reload does not work

                if (m_webView)
                    m_webView->setPageUrl(m_webView->url());
                else
                    m_webEngineView->setPageUrl(m_webEngineView->url());
            });
        buttonsBar->addButton(Qn::ReloadPageButton, reloadButton);
    }

    {
        // Right buttons bar setup
        auto fullscreenButton= createStatisticAwareButton(lit("web_widget_fullscreen"));
        fullscreenButton->setIcon(qnSkin->icon("item/fullscreen.png"));
        fullscreenButton->setToolTip(tr("Fullscreen mode"));
        connect(fullscreenButton, &QnImageButtonWidget::clicked, this,
            [this]()
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
    switch(m_webView ? m_webView->status() : m_webEngineView->status())
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

bool QnWebResourceWidget::eventFilter(QObject* object, QEvent* event)
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

QWebPage* QnWebResourceWidget::page() const
{
    if (m_webView)
        return m_webView->page();
    return nullptr;
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
    NX_ASSERT(m_webView || m_webEngineView);
    static constexpr int kMaxUrlDisplayLength = 96;
    // Truncating URL if it is too long to display properly
    // I could strip query part from URL, but some sites, like youtibe will be stripped too much
    QString details = m_webView ? m_webView->url().toString() : m_webEngineView->url().toString();
    return nx::utils::elideString(details, kMaxUrlDisplayLength);;
}
