#include "web_resource_widget.h"

#include <QtCore/QMetaObject>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QAction>

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

using namespace nx::vms::client::desktop;

QnWebResourceWidget::QnWebResourceWidget(
    QnWorkbenchContext *context,
    QnWorkbenchItem *item,
    QGraphicsItem *parent)
    :
    base_type(context, item, parent),
    m_webEngineView(new GraphicsWebEngineView(resource()->getUrl(), this))
{
    setOption(AlwaysShowName, true);

    m_webEngineView->installEventFilter(this);

    const auto iconButton = titleBar()->leftButtonsBar()->button(Qn::RecordingStatusIconButton);
    const auto contentMargins = QMarginsF(0, iconButton->preferredHeight(), 0, 0);
    const auto webParams = detail::OverlayParams(Visible, OverlayFlag::bindToViewport, BaseLayer,
        contentMargins);

    addOverlayWidget(m_webEngineView, webParams);
    setFocusProxy(m_webEngineView);
    connect(this, &QGraphicsWidget::geometryChanged,
        m_webEngineView, &GraphicsQmlView::updateWindowGeometry);

    setOption(QnResourceWidget::WindowRotationForbidden, true);
    updateTitleText();
    updateInfoText();
    updateDetailsText();

    const auto updateStatusesHandler =
        [this]()
        {
            const auto status = m_webEngineView->status();
            const auto resourceStatus = (status == kPageLoadFailed ? Qn::Offline : Qn::Online);
            resource()->setStatus(resourceStatus);

            updateStatusOverlay(true);
        };

    connect(m_webEngineView, &GraphicsWebEngineView::statusChanged, this, updateStatusesHandler);

    connect(resource(), &QnResource::urlChanged, this,
        [this]()
        {
            m_webEngineView->setUrl(resource()->getUrl());
        });

    setupOverlays();
    updateButtonsVisibility();
}

QnWebResourceWidget::~QnWebResourceWidget()
{
    /* statusChanged will be emitted during the destruction process. */
    m_webEngineView->disconnect(this);
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
        connect(backButton, &QnImageButtonWidget::clicked, m_webEngineView, &GraphicsWebEngineView::back);
        buttonsBar->addButton(Qn::BackButton, backButton);
        buttonsBar->setButtonsEnabled(Qn::BackButton, false);

        connect(m_webEngineView, &GraphicsWebEngineView::canGoBackChanged, this,
            [this, buttonsBar]()
            {
                buttonsBar->setButtonsEnabled(Qn::BackButton, m_webEngineView->canGoBack());
            });

        // Should force HUD to update details text with new URL.
        connect(m_webEngineView, &GraphicsWebEngineView::loadStarted, this,
            [this]()
            {
                this->updateDetailsText();
            });

        auto reloadButton = createStatisticAwareButton(lit("web_widget_reload"));
        reloadButton->setIcon(qnSkin->icon("item/refresh.png"));
        connect(reloadButton, &QnImageButtonWidget::clicked, this,
            [this]()
            {
                // We can't use WebEngineView::reload because if it was an
                // error previously, reload does not work.

                m_webEngineView->setPageUrl(m_webEngineView->url());
            });
        buttonsBar->addButton(Qn::ReloadPageButton, reloadButton);
    }

    {
        // Right buttons bar setup.
        auto fullscreenButton= createStatisticAwareButton(lit("web_widget_fullscreen"));
        fullscreenButton->setIcon(qnSkin->icon("item/fullscreen.png"));
        fullscreenButton->setToolTip(tr("Fullscreen mode"));
        connect(fullscreenButton, &QnImageButtonWidget::clicked, this,
            [this]()
            {
                // Toggles fullscreen item mode.
                const auto newFullscreenItem = (options().testFlag(FullScreenMode) ? nullptr : item());
                workbench()->setItem(Qn::ZoomedRole, newFullscreenItem);
            });
        titleBar()->rightButtonsBar()->addButton(Qn::FullscreenButton, fullscreenButton);
    }
}

Qn::ResourceStatusOverlay QnWebResourceWidget::calculateStatusOverlay() const
{
    switch (m_webEngineView->status())
    {
    case kPageInitialLoadInProgress:
        return Qn::LoadingOverlay;
    //case kPageLoading:
    //case kPageLoaded:
    //case kPageLoadFailed:
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
    if (object == m_webEngineView)
    {
        if (event->type() == QEvent::GraphicsSceneMousePress)
        {
            auto mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
            mousePressEvent(mouseEvent);
            if (mouseEvent->button() == Qt::RightButton)
                return true;
        }
        else if (event->type() == QEvent::GraphicsSceneContextMenu)
            return true;
    }

    return base_type::eventFilter(object, event);
}

void QnWebResourceWidget::triggerWebAction(QWebEnginePage::WebAction action)
{
    if (QQuickItem* webEngineView = m_webEngineView->rootObject())
        QMetaObject::invokeMethod(webEngineView, "triggerAction", Q_ARG(QVariant, action));
}

bool QnWebResourceWidget::isWebActionEnabled(QWebEnginePage::WebAction action)
{
    QQuickItem* webEngineView = m_webEngineView->rootObject();

    if (!webEngineView)
        return false;

    // TODO: Reimplement using WebEngineView.action() after upgrade to Qt 5.12
    switch (action)
    {
        case QWebEnginePage::Back:
            return webEngineView->property("canGoBack").toBool();
        case QWebEnginePage::Forward:
            return webEngineView->property("canGoForward").toBool();
        case QWebEnginePage::Stop:
            return webEngineView->property("loading").toBool();
        default:
            return true;
    }
}

// TODO: Reimplement using WebEngineView.action() after upgrade to Qt 5.12
QString QnWebResourceWidget::webActionText(QWebEnginePage::WebAction action)
{
    const auto item = m_webActionText.find(action);
    if (item != m_webActionText.end())
        return item.value();

    QScopedPointer<QWebEnginePage> tmpPage(new QWebEnginePage());
    const auto text = tmpPage->action(action)->text();
    m_webActionText.insert(action, text);
    return text;
}

// TODO: Reimplement using WebEngineView.action() after upgrade to Qt 5.12
void QnWebResourceWidget::initWebActionText()
{
    static const auto actions = {
        QWebEnginePage::Back,
        QWebEnginePage::Forward,
        QWebEnginePage::Stop,
        QWebEnginePage::Reload,
        QWebEnginePage::SavePage
    };

    QScopedPointer<QWebEnginePage> tmpPage(new QWebEnginePage());
    for (auto action: actions)
        m_webActionText.insert(action, tmpPage->action(action)->text());
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
    NX_ASSERT(m_webEngineView);
    static constexpr int kMaxUrlDisplayLength = 96;
    // Truncating URL if it is too long to display properly
    // I could strip query part from URL, but some sites, like youtibe will be stripped too much
    QString details = m_webEngineView->url().toString();
    return nx::utils::elideString(details, kMaxUrlDisplayLength);
}
