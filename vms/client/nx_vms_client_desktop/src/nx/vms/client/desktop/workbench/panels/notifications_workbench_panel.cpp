#include "notifications_workbench_panel.h"
#include "buttons.h"

#include <chrono>
#include <memory>

#include <QtCore/QModelIndex>
#include <QtCore/QTimer>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>

#include <core/resource/camera_resource.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/instruments/tool_tip_instrument.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/blinking_image_widget.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/graphics/items/notifications/notification_tooltip_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/processors/hover_processor.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_pane_settings.h>

#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/common/widgets/async_image_widget.h>
#include <nx/vms/client/desktop/common/widgets/text_edit_label.h>
#include <nx/vms/client/desktop/event_search/widgets/event_panel.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/event_search/widgets/event_tile.h>
#include <nx/vms/client/desktop/image_providers/camera_thumbnail_provider.h>
#include <nx/vms/client/desktop/image_providers/multi_image_provider.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/widget_utils.h>
#include <nx/vms/client/desktop/workbench/workbench_animations.h>

namespace nx::vms::client::desktop {

using namespace ui;
using namespace std::chrono;

namespace {

static constexpr int kNarrowWidth = 283;
static constexpr int kWideWidth = 433;

static const QSize kToolTipMaxThumbnailSize{240, 180};
static const QSize kToolTipMaxMultiThumbnailSize{240, 540};
static constexpr int kMaxMultiThumbnailCount = 5;
static constexpr int kMultiThumbnailSpacing = 1;
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
    item(new QGraphicsWidget(parentWidget)),
    xAnimator(new VariantAnimator(this)),

    m_showButton(newBlinkingShowHideButton(parentWidget, context(),
        action::ToggleNotificationsAction)),
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
    item->setVisible(false);
    createEventPanel(parentWidget);

    // Hide EventPanel widget when the panel is moved out of view.
    // This is required for correct counting of unread notifications.
    connect(item, &QGraphicsWidget::xChanged, this,
        [this]()
        {
            const bool shouldBeHidden = item->x() >= m_parentWidget->rect().right();
            // Avoid unneeded flickering.
            if (m_eventPanel->isHidden() == shouldBeHidden)
                return;

            // Visibility changes should not affect scene focus. Qt impementation automatically
            // focuses newly appeared graphics item.
            const auto currentFocusItem = m_parentWidget->scene()->focusItem();
            m_eventPanel->setHidden(shouldBeHidden);
            if (shouldBeHidden)
                m_eventPanel->clearFocus();
            else
                m_parentWidget->scene()->setFocusItem(currentFocusItem);
        });

    action(action::PinNotificationsAction)->setChecked(settings.state != Qn::PaneState::Unpinned);
    connect(action(action::PinNotificationsAction), &QAction::toggled, this,
        [this](bool checked)
        {
            setOpened(checked);
            emit geometryChanged();
        });

    action(action::ToggleNotificationsAction)->setChecked(settings.state == Qn::PaneState::Opened);
    m_showButton->setTransform(QTransform::fromScale(-1, 1)); // Achtung! Flips button horizontally.
    m_showButton->setFocusProxy(item);
    m_showButton->setZValue(BackgroundItemZOrder); /*< To make it paint under the tooltip. */
    connect(action(action::ToggleNotificationsAction), &QAction::toggled,
        this, [this](bool opened) { if (!m_blockAction) setOpened(opened); });

    m_opacityProcessor->addTargetItem(item);
    m_opacityProcessor->addTargetItem(m_showButton);
    m_opacityProcessor->addTargetItem(m_eventPanelContainer);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    m_hidingProcessor->addTargetItem(item);
    m_hidingProcessor->addTargetItem(m_showButton);
    m_hidingProcessor->addTargetItem(m_eventPanelContainer);
    m_hidingProcessor->setHoverLeaveDelay(kCloseSidePanelTimeoutMs);
    m_hidingProcessor->setFocusLeaveDelay(kCloseSidePanelTimeoutMs);
    connect(m_hidingProcessor, &HoverFocusProcessor::hoverLeft, this,
        [this]
        {
            if (!isPinned())
                setOpened(false);
        });

    m_showingProcessor->addTargetItem(m_showButton);
    m_showingProcessor->setHoverEnterDelay(kOpenPanelTimeoutMs);
    connect(m_showingProcessor, &HoverFocusProcessor::hoverEntered, this,
        &NotificationsWorkbenchPanel::at_showingProcessor_hoverEntered);

    xAnimator->setTimer(animationTimer());
    xAnimator->setTargetObject(item);
    xAnimator->setAccessor(new PropertyAccessor("x"));

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(item));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(backgroundItem));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_showButton));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_eventPanelContainer));

    /* Create a shadow: */
    auto shadow = new QnEdgeShadowWidget(item, item, Qt::LeftEdge, kShadowThickness);
    shadow->setZValue(ShadowItemZOrder);

    updateControlsGeometry();
}

NotificationsWorkbenchPanel::~NotificationsWorkbenchPanel()
{
    if (m_eventPanelContainer)
        m_eventPanelContainer->setWidget(nullptr);
}

bool NotificationsWorkbenchPanel::isPinned() const
{
    return action(action::PinNotificationsAction)->isChecked();
}

bool NotificationsWorkbenchPanel::isOpened() const
{
    return action(action::ToggleNotificationsAction)->isChecked();
}

void NotificationsWorkbenchPanel::setOpened(bool opened, bool animate)
{
    using namespace ui::workbench;

    ensureAnimationAllowed(&animate);

    m_showingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    auto toggleAction = action(action::ToggleNotificationsAction);
    m_blockAction = true;
    toggleAction->setChecked(opened);
    m_blockAction = false;

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
        opacityAnimator(m_eventPanelContainer)->setTargetValue(opacity);
        m_opacityAnimatorGroup->start();
    }
    else
    {
        m_opacityAnimatorGroup->stop();
        item->setOpacity(opacity);
        backgroundItem->setOpacity(opacity);
        m_eventPanelContainer->setOpacity(opacity);
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

void NotificationsWorkbenchPanel::enableShowButton(bool enabled)
{
    m_showButton->setAcceptedMouseButtons(enabled ? Qt::LeftButton : Qt::NoButton);
}

void NotificationsWorkbenchPanel::updateControlsGeometry()
{
    auto parentWidgetRect = m_parentWidget->rect();

    QRectF paintGeometry = item->geometry();
    backgroundItem->setGeometry(paintGeometry);
    m_showButton->setPos(QPointF(
        qMin(parentWidgetRect.right(), paintGeometry.left()),
        (parentWidgetRect.top() + parentWidgetRect.bottom() - m_showButton->size().height()) / 2));

    emit geometryChanged();
}

void NotificationsWorkbenchPanel::at_showingProcessor_hoverEntered()
{
    if (!isPinned() && !isOpened())
    {
        setOpened(true, true);

        /* So that the click that may follow won't hide it. */
        enableShowButton(false);
        QTimer::singleShot(kButtonInactivityTimeoutMs, this,
            [this]
            {
                enableShowButton(true);
            });
    }

    m_hidingProcessor->forceHoverEnter();
    m_opacityProcessor->forceHoverEnter();
}

void NotificationsWorkbenchPanel::createEventPanel(QGraphicsWidget* parentWidget)
{
    m_eventPanel.reset(new EventPanel(context()));

    // TODO: #vkutin Get rid of proxying.
    m_eventPanelContainer = new QnMaskedProxyWidget(parentWidget);
    m_eventPanelContainer->setProperty(Qn::NoHandScrollOver, true);
    m_eventPanelContainer->setProperty(Qn::BlockMotionSelection, true);
    m_eventPanelContainer->setFocusPolicy(Qt::ClickFocus);

    auto eventPanelResizer = new ResizerWidget(item, m_eventPanelContainer);
    auto dragProcessor = new DragProcessor(this);
    dragProcessor->setHandler(eventPanelResizer);

    connect(item, &QGraphicsWidget::geometryChanged, this,
        [this, eventPanelResizer]()
        {
            // Add 1-pixel shift for notification panel frame.
            const auto newGeometry = item->geometry().adjusted(1, 0, 0, 0);
            m_eventPanelContainer->setGeometry(newGeometry);
            eventPanelResizer->setGeometry(0, 0, style::Metrics::kStandardPadding,
                m_eventPanelContainer->size().height());
        });

    // TODO: #vkutin Test which mode is faster.
    //eventPanelContainer->setCacheMode(QGraphicsItem::NoCache);
    m_eventPanelContainer->setWidget(m_eventPanel.data());

    auto toolTipInstrument = InstrumentManager::instance(parentWidget->scene())
        ->instrument<ToolTipInstrument>();

    if (toolTipInstrument)
        toolTipInstrument->addIgnoredItem(m_eventPanelContainer);

    connect(m_eventPanel.data(), &EventPanel::tileHovered,
        this, &NotificationsWorkbenchPanel::at_eventTileHovered);

    connect(m_eventPanel.data(), &EventPanel::unreadCountChanged, this,
        [this](int count, QnNotificationLevel::Value level)
        {
            m_showButton->setNotificationCount(count);
            m_showButton->setColor(QnNotificationLevel::notificationColor(level));
        });

    connect(m_eventPanel.data(), &EventPanel::currentTabChanged, this,
        [this](EventPanel::Tab tab)
        {
            if (tab == EventPanel::Tab::motion && !display()->widget(Qn::ZoomedRole))
                setOpened();
        });
}

void NotificationsWorkbenchPanel::at_eventTileHovered(
    const QModelIndex& index,
    EventTile* tile)
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

    auto multiImageProvider = this->multiImageProvider(index);
    const auto imageProvider = multiImageProvider ? multiImageProvider.get() : tile->preview();

    auto text = tile->toolTip();
    if (text.isEmpty() && imageProvider)
        text = tile->title();
    if (text.isEmpty())
        return;

    const auto tilePos = (tile->rect().topLeft() + tile->rect().bottomLeft()) / 2;
    const auto globalPos = QnHiDpiWorkarounds::safeMapToGlobal(tile, tilePos);
    const auto tooltipPos = WidgetUtils::mapFromGlobal(m_parentWidget, globalPos);

    QPointer<QnNotificationToolTipWidget> toolTip(
        new QnNotificationToolTipWidget(m_parentWidget));

    toolTip->setOpacity(0.0);
    toolTip->setEnclosingGeometry(tooltipsEnclosingRect);
    toolTip->setMaxThumbnailSize(multiImageProvider
        ? kToolTipMaxMultiThumbnailSize
        : kToolTipMaxThumbnailSize);

    toolTip->setText(text);
    toolTip->setImageProvider(imageProvider);
    toolTip->setHighlightRect(multiImageProvider ? QRectF() : tile->previewCropRect());
    toolTip->setThumbnailVisible(imageProvider != nullptr);
    toolTip->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true);
    toolTip->updateTailPos();
    toolTip->pointTo(tooltipPos);
    toolTip->setCropMode(AsyncImageWidget::CropMode::never);

    // Limit tooltip width to half of the scene width.
    // If tooltip is larger then enable word wrap at half of the scene length.
    // The tooltip cannot be displayed outside of the scene, so if the tooltip goes beyound the
    // scene edge, use all available space except a small margin.

    static constexpr auto kToolTipMargin = 8; //< Leave some space to the window edge.

    // toolTip has not been shown yet, so we can get layout paddings from its size.
    const int widthWithoutText = toolTip->size().width();

    // Calculate tooltip maximum allowed width.
    const auto size = toolTip->scene()->views().first()->size();
    const int maxToolTipWidth = std::min(
        size.width() / 2 - widthWithoutText,
        size.width() - m_eventPanel->size().width() - kToolTipMargin - widthWithoutText);

    // If the tooltip with disabled word wrapping is too wide, enable word wrapping at max width.
    if (toolTip->textLabel()->document()->idealWidth() > maxToolTipWidth)
    {
        toolTip->textLabel()->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        toolTip->textLabel()->setLineWrapMode(QTextEdit::FixedPixelWidth);
        toolTip->textLabel()->setLineWrapColumnOrWidth(maxToolTipWidth);
        toolTip->updateTailPos();
    }

    // To simplify the code, just limit the height of the text under tooltip thumbnail to be 1/2 of
    // the scene height.
    const int heightWithoutText =
        toolTip->size().height() + (imageProvider ? size.height() / 2 : 0);

    WidgetUtils::elideDocumentHeight(
        toolTip->textLabel()->document(),
        size.height() - heightWithoutText - 2 * kToolTipMargin);

    if (multiImageProvider)
    {
        multiImageProvider->loadAsync();
        multiImageProvider.release()->setParent(toolTip);
    }

    // TODO: #vkutin Refactor tooltip clicks, now it looks hackish.
    connect(toolTip.data(), &QnNotificationToolTipWidget::thumbnailClicked, tile,
        [tile]() { emit tile->clicked(Qt::LeftButton, QApplication::keyboardModifiers()); });

    connect(tile, &EventTile::clicked, this,
        [this]()
        {
            if (m_eventPanelHoverProcessor)
                m_eventPanelHoverProcessor->forceHoverLeave();
        });

    m_eventPanelHoverProcessor = new HoverFocusProcessor(m_eventPanel->graphicsProxyWidget());
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
            connect(animator, &VariantAnimator::finished,
                toolTip.data(), &QObject::deleteLater);
            animator->animateTo(0.0);
        });

    m_eventPanelHoverProcessor->forceHoverEnter();
}

std::unique_ptr<MultiImageProvider> NotificationsWorkbenchPanel::multiImageProvider(
    const QModelIndex& index) const
{
    const auto previewTime = index.data(Qn::PreviewTimeRole).value<microseconds>();

    const auto requiredPermission = previewTime > 0ms
        ? Qn::ViewFootagePermission
        : Qn::ViewLivePermission;

    const auto cameras = index.data(Qn::ResourceListRole).value<QnResourceList>()
        .filtered<QnVirtualCameraResource>(
            [this, requiredPermission](const QnVirtualCameraResourcePtr& camera)
            {
                return accessController()->hasPermissions(camera, requiredPermission);
            });

    if (cameras.size() < 2)
        return {};

    const bool precisePreview = index.data(Qn::ForcePrecisePreviewRole).toBool();
    const auto streamSelectionMode = index.data(Qn::PreviewStreamSelectionRole)
        .value<nx::api::CameraImageRequest::StreamSelectionMode>();

    MultiImageProvider::Providers providers;

    nx::api::CameraImageRequest request;
    request.size = QSize(kToolTipMaxThumbnailSize.width(), 0);
    request.imageFormat = nx::api::ImageRequest::ThumbnailFormat::jpg;
    request.aspectRatio = nx::api::ImageRequest::AspectRatio::auto_;
    request.streamSelectionMode = streamSelectionMode;
    request.usecSinceEpoch =
        previewTime.count() > 0 ? previewTime.count() : nx::api::ImageRequest::kLatestThumbnail;
    request.roundMethod = precisePreview
        ? nx::api::ImageRequest::RoundMethod::precise
        : nx::api::ImageRequest::RoundMethod::iFrameAfter;

    for (const auto& camera: cameras)
    {
        if (providers.size() >= kMaxMultiThumbnailCount)
            break;

        request.camera = camera;
        providers.emplace_back(new CameraThumbnailProvider(request));
    }

    return std::make_unique<MultiImageProvider>(
        std::move(providers), Qt::Vertical, kMultiThumbnailSpacing);
}

} //namespace nx::vms::client::desktop
