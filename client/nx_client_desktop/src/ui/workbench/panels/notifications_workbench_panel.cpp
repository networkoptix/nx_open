#include "notifications_workbench_panel.h"

#include <QtCore/QTimer>

#include <QtWidgets/QAction>

#include <ini.h>
#include <nx/client/desktop/ui/workbench/workbench_animations.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/instruments/tool_tip_instrument.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/blinking_image_widget.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/graphics/items/notifications/notifications_collection_widget.h>
#include <ui/graphics/items/notifications/notification_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/panels/buttons.h>

#include <utils/common/scoped_value_rollback.h>

#include <nx/client/desktop/event_search/widgets/event_panel.h>
#include <nx/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/client/desktop/event_search/widgets/event_tile.h>
#include <nx/client/desktop/utils/widget_utils.h>

using namespace nx::client::desktop;
using namespace nx::client::desktop::ui;

namespace NxUi {

namespace {

static constexpr int kNarrowWidth = 280;
static constexpr int kWideWidth = 430;

static const QSize kToolTipMaxThumbnailSize(480, 480);
static constexpr int kToolTipShowDelayMs = 250;
static constexpr int kToolTipHideDelayMs = 250;
static constexpr qreal kToolTipFadeSpeedFactor = 2.0;

class ResizerWidget: public QGraphicsWidget, public DragProcessHandler
{
public:
    ResizerWidget(QGraphicsWidget* toResize, QGraphicsItem* parent):
        QGraphicsWidget(parent),
        DragProcessHandler(),
        m_itemToResize(toResize)
    {
        NX_ASSERT(m_itemToResize);

        setCursor(Qt::SizeHorCursor);
        setFlag(QGraphicsItem::ItemHasNoContents);
    }

protected:
    virtual void dragMove(DragInfo* info) override
    {
        if (!m_itemToResize)
            return;

        const auto delta = info->mouseItemPos().x() - info->mousePressItemPos().x();
        const auto newWidth = m_itemToResize->minimumWidth() - delta;

        static constexpr auto kThreshold = (kNarrowWidth + kWideWidth) / 2;
        static constexpr int kBoundaryWidth = 8;

        int newMinimumWidth = m_itemToResize->minimumWidth();

        if (newWidth > kThreshold + kBoundaryWidth)
            newMinimumWidth = kWideWidth;
        else if (newWidth < kThreshold - kBoundaryWidth)
            newMinimumWidth = kNarrowWidth;

        const int sizeDelta = m_itemToResize->minimumWidth() - newMinimumWidth;
        if (sizeDelta == 0)
            return;

        const QRect newGeometry(
            m_itemToResize->pos().x() + sizeDelta,
            m_itemToResize->pos().y(),
            newMinimumWidth,
            m_itemToResize->size().height());

        m_itemToResize->setMinimumWidth(newMinimumWidth);
        m_itemToResize->setGeometry(newGeometry);
    }

    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        dragProcessor()->mousePressEvent(this, event);
        event->accept();
    }

    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
    {
        dragProcessor()->mouseMoveEvent(this, event);
        event->accept();
    }

    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
        dragProcessor()->mouseReleaseEvent(this, event);
        event->accept();
    }

private:
    QGraphicsWidget* const m_itemToResize = nullptr;
};

} // namespace

NotificationsWorkbenchPanel::NotificationsWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    backgroundItem(new QnControlBackgroundWidget(Qt::RightEdge, parentWidget)),
    item(new QnNotificationsCollectionWidget(parentWidget, 0, context())),
    xAnimator(new VariantAnimator(this)),

    m_showButton(NxUi::newBlinkingShowHideButton(parentWidget, context(),
        action::PinNotificationsAction)),
    m_hidingProcessor(new HoverFocusProcessor(parentWidget)),
    m_showingProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityProcessor(new HoverFocusProcessor(parentWidget)),
    m_opacityAnimatorGroup(new AnimatorGroup(this))
{
    /* Notifications panel. */
    backgroundItem->setFrameBorders(Qt::LeftEdge);
    backgroundItem->setZValue(BackgroundItemZOrder);

    item->setZValue(ContentItemZOrder);
    item->setProperty(Qn::NoHandScrollOver, true);
    item->setProperty(Qn::BlockMotionSelection, true);
    setHelpTopic(item, Qn::MainWindow_Notifications_Help);
    connect(item, &QGraphicsWidget::geometryChanged, this,
        &NotificationsWorkbenchPanel::updateControlsGeometry);

    item->setMinimumWidth(kNarrowWidth);

    if (nx::client::desktop::ini().unifiedEventPanel)
    {
        item->setVisible(false);
        createEventPanel(parentWidget);
    }

    action(action::PinNotificationsAction)->setChecked(settings.state != Qn::PaneState::Unpinned);
    connect(action(action::PinNotificationsAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (checked)
                setOpened(true);
            emit geometryChanged();
        });

    m_opened = (settings.state == Qn::PaneState::Opened);
    setShowButtonIcon();
    m_showButton->setTransform(QTransform::fromScale(-1, 1));
    m_showButton->setFocusProxy(item);
    m_showButton->setZValue(BackgroundItemZOrder); /*< To make it paint under the tooltip. */
    setHelpTopic(m_showButton, Qn::MainWindow_Pin_Help);
    item->setBlinker(m_showButton);

    m_opacityProcessor->addTargetItem(item);
    m_opacityProcessor->addTargetItem(m_showButton);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    m_hidingProcessor->addTargetItem(item);
    m_hidingProcessor->addTargetItem(m_showButton);
    m_hidingProcessor->setHoverLeaveDelay(NxUi::kClosePanelTimeoutMs);
    m_hidingProcessor->setFocusLeaveDelay(NxUi::kClosePanelTimeoutMs);
    connect(m_hidingProcessor, &HoverFocusProcessor::hoverLeft, this,
        [this]
        {
            if (!isPinned())
                setOpened(false);
        });

    m_showingProcessor->addTargetItem(m_showButton);
    m_showingProcessor->setHoverEnterDelay(NxUi::kOpenPanelTimeoutMs);
    connect(m_showingProcessor, &HoverFocusProcessor::hoverEntered, this,
        &NotificationsWorkbenchPanel::at_showingProcessor_hoverEntered);

    xAnimator->setTimer(animationTimer());
    xAnimator->setTargetObject(item);
    xAnimator->setAccessor(new PropertyAccessor("x"));

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(item));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(backgroundItem));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_showButton));

    /* Create a shadow: */
    auto shadow = new QnEdgeShadowWidget(item, item, Qt::LeftEdge, NxUi::kShadowThickness);
    shadow->setZValue(NxUi::ShadowItemZOrder);

    updateControlsGeometry();
}

NotificationsWorkbenchPanel::~NotificationsWorkbenchPanel()
{
}

bool NotificationsWorkbenchPanel::isPinned() const
{
    return action(action::PinNotificationsAction)->isChecked();
}

bool NotificationsWorkbenchPanel::isOpened() const
{
    return m_opened;;
}

void NotificationsWorkbenchPanel::setOpened(bool opened, bool animate)
{
    using namespace nx::client::desktop::ui::workbench;

    ensureAnimationAllowed(&animate);

    m_showingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    m_opened = opened;

    xAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(xAnimator, opened
        ? Animations::Id::NotificationsPanelExpand
        : Animations::Id::NotificationsPanelCollapse);

    qreal width = item->size().width();
    qreal newX = m_parentWidget->rect().right()
        + (opened ? -width : kHidePanelOffset);

    if (animate)
        xAnimator->animateTo(newX);
    else
        item->setX(newX);

    setShowButtonIcon();

    emit openedChanged(opened, animate);
}

bool NotificationsWorkbenchPanel::isVisible() const
{
    return m_visible;
}

void NotificationsWorkbenchPanel::setVisible(bool visible, bool animate)
{
    ensureAnimationAllowed(&animate);

    bool changed = m_visible != visible;

    m_visible = visible;

    updateOpacity(animate);
    if (changed)
        emit visibleChanged(visible, animate);
}

qreal NotificationsWorkbenchPanel::opacity() const
{
    return opacityAnimator(item)->targetValue().toDouble();
}

void NotificationsWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (animate)
    {
        m_opacityAnimatorGroup->pause();
        opacityAnimator(item)->setTargetValue(opacity);
        opacityAnimator(backgroundItem)->setTargetValue(opacity);
        opacityAnimator(m_showButton)->setTargetValue(opacity);
        m_opacityAnimatorGroup->start();
    }
    else
    {
        m_opacityAnimatorGroup->stop();
        item->setOpacity(opacity);
        backgroundItem->setOpacity(opacity);
        m_showButton->setOpacity(opacity);
    }
}

bool NotificationsWorkbenchPanel::isHovered() const
{
    return m_opacityProcessor->isHovered();
}

QRectF NotificationsWorkbenchPanel::effectiveGeometry() const
{
    QRectF geometry = item->geometry();
    if (xAnimator->isRunning())
        geometry.moveLeft(xAnimator->targetValue().toReal());
    return geometry;
}

void NotificationsWorkbenchPanel::stopAnimations()
{
    if (!xAnimator->isRunning())
        return;

    xAnimator->stop();
    item->setX(xAnimator->targetValue().toDouble());
}

void NotificationsWorkbenchPanel::setShowButtonUsed(bool used)
{
    m_showButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void NotificationsWorkbenchPanel::setShowButtonIcon()
{
    m_showButton->setIcon(qnSkin->icon(m_opened
        ? "panel/slide_pin.png"
        : "panel/slide_right.png",
        "panel/slide_left.png"));
}

void NotificationsWorkbenchPanel::updateControlsGeometry()
{
    auto parentWidgetRect = m_parentWidget->rect();
    QRectF headerGeometry = m_parentWidget->mapRectFromItem(item, item->headerGeometry());
    QRectF backgroundGeometry = m_parentWidget->mapRectFromItem(item, item->visibleGeometry());

    QRectF paintGeometry = item->geometry();
    backgroundItem->setGeometry(paintGeometry);
    m_showButton->setPos(QPointF(
        qMin(parentWidgetRect.right(), paintGeometry.left()),
        (parentWidgetRect.top() + parentWidgetRect.bottom() - m_showButton->size().height()) / 2
    ));

    emit geometryChanged();
}

void NotificationsWorkbenchPanel::at_showingProcessor_hoverEntered()
{
    if (!isPinned() && !isOpened())
    {
        setOpened(true, true);

        /* So that the click that may follow won't hide it. */
        setShowButtonUsed(false);
        QTimer::singleShot(NxUi::kButtonInactivityTimeoutMs, this,
            [this]
            {
                setShowButtonUsed(true);
            });
    }

    m_hidingProcessor->forceHoverEnter();
    m_opacityProcessor->forceHoverEnter();
}

void NotificationsWorkbenchPanel::createEventPanel(QGraphicsWidget* parentWidget)
{
    using namespace nx::client::desktop;

    m_eventPanel.reset(new EventPanel(context()));

    // TODO: #vkutin Get rid of proxying.
    auto eventPanelContainer = new QnMaskedProxyWidget(parentWidget);

    auto eventPanelResizer = new ResizerWidget(item, eventPanelContainer);
    auto dragProcessor = new DragProcessor(this);
    dragProcessor->setHandler(eventPanelResizer);

    connect(item, &QGraphicsWidget::geometryChanged, this,
        [this, eventPanelContainer, eventPanelResizer]()
        {
            // Add 1-pixel shift for notification panel frame.
            const auto newGeometry = item->geometry().adjusted(1, 0, 0, 0);
            eventPanelContainer->setGeometry(newGeometry);
            eventPanelResizer->setGeometry(0, 0, style::Metrics::kStandardPadding,
                eventPanelContainer->size().height());
        });

    // TODO: #vkutin Test which mode is faster.
    //eventPanelContainer->setCacheMode(QGraphicsItem::NoCache);
    eventPanelContainer->setWidget(m_eventPanel.data());

    m_hidingProcessor->addTargetItem(eventPanelContainer);

    auto toolTipInstrument = InstrumentManager::instance(parentWidget->scene())
        ->instrument<ToolTipInstrument>();

    if (toolTipInstrument)
    {
        const auto ribbons = m_eventPanel->findChildren<EventRibbon*>();
        for (auto ribbon: ribbons)
            toolTipInstrument->addIgnoredWidget(ribbon);
    }

    connect(m_eventPanel.data(), &EventPanel::tileHovered,
        this, &NotificationsWorkbenchPanel::at_eventTileHovered);
}

void NotificationsWorkbenchPanel::at_eventTileHovered(
    const QModelIndex& index,
    const nx::client::desktop::EventTile* tile)
{
    if (m_eventPanelHoverProcessor)
    {
        if (m_lastHoveredTile == tile)
            m_eventPanelHoverProcessor->forceHoverEnter();
        else
            m_eventPanelHoverProcessor->forceHoverLeave();
    }

    if (!tile || !index.isValid())
    {
        m_eventPanelHoverProcessor = nullptr;
        return;
    }

    m_lastHoveredTile = tile;

    const auto parentWidget = m_eventPanel->graphicsProxyWidget();
    const auto imageProvider = tile->preview();
    const auto text = tile->toolTip().isEmpty() ? tile->title() : tile->toolTip();

    const auto tilePos = (tile->rect().topLeft() + tile->rect().bottomLeft()) / 2;
    const auto globalPos = QnHiDpiWorkarounds::safeMapToGlobal(tile, tilePos);
    const auto tooltipPos = WidgetUtils::mapFromGlobal(parentWidget, globalPos);

    QPointer<QnNotificationToolTipWidget> toolTip(
        new QnNotificationToolTipWidget(parentWidget));
    toolTip->setOpacity(0.0);
    toolTip->setEnclosingGeometry(item->toolTipsEnclosingRect());
    toolTip->setMaxThumbnailSize(kToolTipMaxThumbnailSize);
    toolTip->setText(text);
    toolTip->setImageProvider(imageProvider);
    toolTip->setHighlightRect(tile->previewCropRect());
    toolTip->setThumbnailVisible(imageProvider != nullptr);
    toolTip->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
    toolTip->updateTailPos();
    toolTip->pointTo(tooltipPos);

    connect(toolTip.data(), &QnNotificationToolTipWidget::thumbnailClicked,
        tile, &nx::client::desktop::EventTile::clicked);

    m_eventPanelHoverProcessor = new HoverFocusProcessor(parentWidget);
    m_eventPanelHoverProcessor->addTargetItem(toolTip);
    m_eventPanelHoverProcessor->setHoverEnterDelay(kToolTipShowDelayMs);
    m_eventPanelHoverProcessor->setHoverLeaveDelay(kToolTipHideDelayMs);

    connect(m_eventPanelHoverProcessor.data(), &HoverFocusProcessor::hoverEntered, this,
        [this, toolTip]()
        {
            if (toolTip)
                opacityAnimator(toolTip, kToolTipFadeSpeedFactor)->animateTo(1.0);
        });

    connect(m_eventPanelHoverProcessor.data(), &HoverFocusProcessor::hoverLeft, this,
        [this, toolTip]
        {
            if (m_eventPanelHoverProcessor == sender())
                m_eventPanelHoverProcessor = nullptr;

            sender()->deleteLater();

            if (!toolTip)
                return;

            auto animator = opacityAnimator(toolTip, kToolTipFadeSpeedFactor);
            animator->animateTo(0.0);
            connect(animator, &VariantAnimator::finished,
                toolTip.data(), &QObject::deleteLater);
        });

    m_eventPanelHoverProcessor->forceHoverEnter();
}

} //namespace NxUi
