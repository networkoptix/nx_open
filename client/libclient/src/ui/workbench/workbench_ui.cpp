#include "workbench_ui.h"

#include <cmath> /* For std::floor. */

#include <QtCore/QSettings>
#include <QtCore/QTimer>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QStyle>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <common/common_module.h>

#include <core/resource/security_cam_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_bookmark.h>

#include <camera/resource_display.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>

#include <ui/animation/viewport_animator.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>

#include <ui/common/palette.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/animation_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/bounding_instrument.h>
#include <ui/graphics/instruments/activity_listener_instrument.h>
#include <ui/graphics/instruments/fps_counting_instrument.h>
#include <ui/graphics/instruments/drop_instrument.h>
#include <ui/graphics/instruments/focus_listener_instrument.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/framed_widget.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/generic/ui_elements_widget.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/generic/resizer_widget.h>
#include <ui/graphics/items/controls/navigation_item.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/controls/speed_slider.h>
#include <ui/graphics/items/controls/volume_slider.h>
#include <ui/graphics/items/controls/time_scroll_bar.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/web_view.h>
#include <ui/graphics/items/controls/bookmarks_viewer.h>
#include <ui/graphics/items/notifications/notifications_collection_widget.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/processors/hover_processor.h>

#include <ui/screen_recording/screen_recorder.h>

#include <ui/statistics/modules/controls_statistics_module.h>

#include <ui/style/skin.h>

#include <ui/widgets/calendar_widget.h>
#include <ui/widgets/day_time_widget.h>
#include <ui/widgets/resource_browser_widget.h>
#include <ui/widgets/layout_tab_bar.h>
#include <ui/widgets/main_window.h>
#include <ui/widgets/main_window_title_bar_widget.h>

#include <ui/workaround/qtbug_workaround.h>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/checked_cast.h>
#include <utils/common/counter.h>

#include "watchers/workbench_render_watcher.h"
#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/ui/buttons.h>

#include "workbench.h"
#include "workbench_display.h"
#include "workbench_layout.h"
#include "workbench_context.h"
#include "workbench_navigator.h"
#include "workbench_access_controller.h"
#include "workbench_pane_settings.h"

#include <nx/fusion/model_functions.h>

#ifdef _DEBUG
//#define QN_DEBUG_WIDGET
#endif


namespace {

Qn::PaneState makePaneState(bool opened, bool pinned = true)
{
    return pinned ? (opened ? Qn::PaneState::Opened : Qn::PaneState::Closed) : Qn::PaneState::Unpinned;
}

const qreal kOpaque = 1.0;
const qreal kHidden = 0.0;

const int kHideControlsTimeoutMs = 2000;
const int kShowTimelineTimeoutMs = 100;
const int kCloseTimelineTimeoutMs = 250;

const int kVideoWallTimelineAutoHideTimeoutMs = 10000;

const int kButtonInactivityTimeoutMs = 300;

} // anonymous namespace

#ifdef QN_DEBUG_WIDGET
static QtMessageHandler previousMsgHandler = 0;
static QnDebugProxyLabel* debugLabel = 0;

static void uiMsgHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (previousMsgHandler)
        previousMsgHandler(type, ctx, msg);

    if (!debugLabel)
        return;

    debugLabel->appendTextQueued(msg);
}
#endif

QnWorkbenchUi::QnWorkbenchUi(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_instrumentManager(display()->instrumentManager()),
    m_fpsCountingInstrument(nullptr),
    m_controlsActivityInstrument(nullptr),
    m_flags(0),
    m_controlsWidget(nullptr),
    m_treeVisible(false),
    m_titleUsed(false),
    m_titleVisible(false),
    m_notificationsVisible(false),
    m_calendarVisible(false),
    m_dayTimeOpened(false),
    m_ignoreClickEvent(false),
    m_inactive(false),
    m_fpsItem(nullptr),
    m_debugOverlayLabel(nullptr),

    m_inFreespace(false),
    m_unzoomedOpenedPanels(),

    m_timeline(),
    m_tree(),

    m_titleItem(nullptr),
    m_titleShowButton(nullptr),
    m_titleOpacityAnimatorGroup(nullptr),
    m_titleYAnimator(nullptr),
    m_titleOpacityProcessor(nullptr),

    m_notificationsBackgroundItem(nullptr),
    m_notificationsItem(nullptr),
    m_notificationsPinButton(nullptr),
    m_notificationsShowButton(nullptr),
    m_notificationsOpacityProcessor(nullptr),
    m_notificationsHidingProcessor(nullptr),
    m_notificationsShowingProcessor(nullptr),
    m_notificationsXAnimator(nullptr),
    m_notificationsOpacityAnimatorGroup(nullptr),

    m_calendarItem(nullptr),
    m_calendarPinButton(nullptr),
    m_dayTimeMinimizeButton(nullptr),
    m_calendarSizeAnimator(nullptr),
    m_calendarOpacityAnimatorGroup(nullptr),
    m_calendarOpacityProcessor(nullptr),
    m_calendarHidingProcessor(nullptr),
    m_calendarShowingProcessor(nullptr),
    m_inCalendarGeometryUpdate(false),

    m_inDayTimeGeometryUpdate(false),
    m_dayTimeItem(nullptr),
    m_dayTimeWidget(nullptr),
    m_dayTimeSizeAnimator(nullptr),

    m_calendarPinOffset(),
    m_dayTimeOffset()
{
    m_widgetByRole.fill(nullptr);

    QGraphicsLayout::setInstantInvalidatePropagation(true);

    /* Install and configure instruments. */
    m_fpsCountingInstrument = new FpsCountingInstrument(1000, this);
    m_controlsActivityInstrument = new ActivityListenerInstrument(true, kHideControlsTimeoutMs, this);

    m_instrumentManager->installInstrument(m_fpsCountingInstrument, InstallationMode::InstallBefore, display()->paintForwardingInstrument());
    m_instrumentManager->installInstrument(m_controlsActivityInstrument);

    connect(m_controlsActivityInstrument, &ActivityListenerInstrument::activityStopped, this, &QnWorkbenchUi::at_activityStopped);
    connect(m_controlsActivityInstrument, &ActivityListenerInstrument::activityResumed, this, &QnWorkbenchUi::at_activityStarted);
    connect(m_fpsCountingInstrument, &FpsCountingInstrument::fpsChanged, this, [this](qreal fps)
    {
#ifdef QN_SHOW_FPS_MS
        QString fmt = lit("%1 (%2ms)");
        m_fpsItem->setText(fmt.arg(QString::number(fps, 'g', 4)).arg(QString::number(1000 / fps, 'g', 4)));
#else
        m_fpsItem->setText(QString::number(fps, 'g', 4));
#endif
        m_fpsItem->resize(m_fpsItem->effectiveSizeHint(Qt::PreferredSize));
    });

    /* Create controls. */
    createControlsWidget();

    /* Animation. */
    setTimer(m_instrumentManager->animationTimer());
    startListening();

    /* Fps counter. */
    createFpsWidget();

    QnPaneSettingsMap settings = qnSettings->paneSettings();

    /* Tree panel. */
    if (!qnRuntime->isActiveXMode())
        createTreeWidget(settings[Qn::WorkbenchPane::Tree]);

    /* Title bar. */
    if (!qnRuntime->isActiveXMode())
        createTitleWidget(settings[Qn::WorkbenchPane::Title]);

    /* Notifications. */
    if (!(qnSettings->lightMode() & Qn::LightModeNoNotifications))
        createNotificationsWidget(settings[Qn::WorkbenchPane::Notifications]);

    /* Calendar. */
    createCalendarWidget(settings[Qn::WorkbenchPane::Calendar]);

    /* Navigation slider. */
    createSliderWidget(settings[Qn::WorkbenchPane::Navigation]);

    /* Windowed title shadow. */
    auto windowedTitleShadow = new QnEdgeShadowWidget(m_controlsWidget,
        Qt::TopEdge, NxUi::kShadowThickness, true);

#ifdef QN_DEBUG_WIDGET
    /* Debug overlay */
    createDebugWidget();
#endif

    initGraphicsMessageBox();

    /* Connect to display. */
    display()->view()->addAction(action(QnActions::FreespaceAction));
    connect(display(), &QnWorkbenchDisplay::viewportGrabbed, this, &QnWorkbenchUi::disableProxyUpdates);
    connect(display(), &QnWorkbenchDisplay::viewportUngrabbed, this, &QnWorkbenchUi::enableProxyUpdates);
    connect(display(), &QnWorkbenchDisplay::widgetChanged, this, &QnWorkbenchUi::at_display_widgetChanged);

    connect(action(QnActions::FreespaceAction), &QAction::triggered, this, &QnWorkbenchUi::at_freespaceAction_triggered);
    connect(action(QnActions::EffectiveMaximizeAction), &QAction::triggered, this,
        [this, windowedTitleShadow]()
        {
            if (m_inFreespace)
                at_freespaceAction_triggered();

            windowedTitleShadow->setVisible(
                !action(QnActions::EffectiveMaximizeAction)->isChecked());
        });

    /* Init fields. */
    setFlags(HideWhenNormal | HideWhenZoomed | AdjustMargins);

    setSliderVisible(false, false);
    setTreeVisible(true, false);
    setTitleVisible(true, false);
    setTitleUsed(false);
    setNotificationsVisible(true, false);
    setDayTimeWidgetOpened(false, false);
    setCalendarVisible(false);
    updateControlsVisibility(false);

    setTreeOpened(settings[Qn::WorkbenchPane::Tree].state == Qn::PaneState::Opened, false);
    setTitleOpened(settings[Qn::WorkbenchPane::Title].state == Qn::PaneState::Opened, false);
    setSliderOpened(settings[Qn::WorkbenchPane::Navigation].state != Qn::PaneState::Closed, false);
    setNotificationsOpened(settings[Qn::WorkbenchPane::Notifications].state == Qn::PaneState::Opened, false);
    setCalendarOpened(settings[Qn::WorkbenchPane::Calendar].state == Qn::PaneState::Opened, false);

    m_timeline.lastThumbnailsHeight = settings[Qn::WorkbenchPane::Thumbnails].span;
    setThumbnailsVisible(settings[Qn::WorkbenchPane::Thumbnails].state == Qn::PaneState::Opened);

    connect(action(QnActions::BeforeExitAction), &QAction::triggered, this,
        [this]
        {
            if (!m_inFreespace)
                storeSettings();
        });
}

QnWorkbenchUi::~QnWorkbenchUi()
{
#ifdef QN_DEBUG_WIDGET
    debugLabel = 0;
#endif // QN_DEBUG_WIDGET

    /* The disconnect call is needed so that our methods don't get triggered while
     * the ui machinery is shutting down. */
    disconnectAll();

    delete m_controlsWidget;
}

void QnWorkbenchUi::storeSettings()
{
    QnPaneSettingsMap settings;
    QnPaneSettings& title = settings[Qn::WorkbenchPane::Title];
    title.state = makePaneState(isTitleOpened());

    QnPaneSettings& tree = settings[Qn::WorkbenchPane::Tree];
    tree.state = makePaneState(isTreeOpened(), isTreePinned());
    tree.span = m_tree.item->geometry().width();

    QnPaneSettings& notifications = settings[Qn::WorkbenchPane::Notifications];
    notifications.state = makePaneState(isNotificationsOpened(), isNotificationsPinned());

    QnPaneSettings& navigation = settings[Qn::WorkbenchPane::Navigation];
    if (m_timeline.pinned)
        navigation.state = Qn::PaneState::Opened;
    else
        navigation.state = m_timeline.opened ? Qn::PaneState::Unpinned : Qn::PaneState::Closed;

    QnPaneSettings& calendar = settings[Qn::WorkbenchPane::Calendar];
    calendar.state = makePaneState(isCalendarOpened(), isCalendarPinned());

    QnPaneSettings& thumbnails = settings[Qn::WorkbenchPane::Thumbnails];
    thumbnails.state = makePaneState(isThumbnailsVisible());
    thumbnails.span = m_timeline.lastThumbnailsHeight;

    qnSettings->setPaneSettings(settings);
}

void QnWorkbenchUi::updateCursor()
{
#ifndef Q_OS_MACX
    bool curtained = m_inactive;
    if (display()->view())
        display()->view()->viewport()->setCursor(QCursor(curtained ? Qt::BlankCursor : Qt::ArrowCursor));
#endif
}

Qn::ActionScope QnWorkbenchUi::currentScope() const
{
    QGraphicsItem *focusItem = display()->scene()->focusItem();
    if (focusItem == m_tree.item)
        return Qn::TreeScope;

    if (focusItem == m_timeline.item)
        return Qn::SliderScope;

    if (!focusItem || dynamic_cast<QnResourceWidget*>(focusItem))
        return Qn::SceneScope;

    /* We should not handle any button as an action while the item was focused. */
    if (dynamic_cast<QnWebView*>(focusItem))
        return Qn::InvalidScope;

    return Qn::MainScope;
}

QnActionParameters QnWorkbenchUi::currentParameters(Qn::ActionScope scope) const
{
    /* Get items. */
    switch (scope)
    {
        case Qn::TreeScope:
            return m_tree.widget ? m_tree.widget->currentParameters(scope) : QnActionParameters();
        case Qn::SliderScope:
            return QnActionParameters(navigator()->currentWidget());
        case Qn::SceneScope:
            return QnActionParameters(QnActionParameterTypes::widgets(display()->scene()->selectedItems()));
        default:
            break;
    }
    return QnActionParameters();
}


QnWorkbenchUi::Flags QnWorkbenchUi::flags() const
{
    return m_flags;
}

void QnWorkbenchUi::setProxyUpdatesEnabled(bool updatesEnabled)
{
    //TODO: #GDM #optimization disable other masked proxy widgets as well
    if (m_tree.item)
        m_tree.item->setUpdatesEnabled(updatesEnabled);
}

void QnWorkbenchUi::enableProxyUpdates()
{
    setProxyUpdatesEnabled(true);
}

void QnWorkbenchUi::disableProxyUpdates()
{
    setProxyUpdatesEnabled(false);
}

void QnWorkbenchUi::updateControlsVisibility(bool animate)
{    // TODO
    ensureAnimationAllowed(animate);

    if (qnRuntime->isVideoWallMode())
    {
        bool sliderVisible =
            navigator()->currentWidget() != nullptr &&
            !(navigator()->currentWidget()->resource()->flags() & (Qn::still_image | Qn::server));

        setSliderVisible(sliderVisible, animate);
        setTreeVisible(false, false);
        setTitleVisible(false, false);
        setNotificationsVisible(false, false);
        return;
    }

    const auto calculateSliderVisible = [this]()
        {
            if (action(QnActions::ToggleTourModeAction)->isChecked())
                return false;

            if (!navigator()->currentWidget())
                return false;

            const auto resource = navigator()->currentWidget()->resource();
            if (!resource)
                return false;

            const auto flags = resource->flags();

            if (flags & (Qn::still_image | Qn::server | Qn::videowall))  /* Any of the flags is sufficient. */
                return false;

            if ((flags & Qn::web_page) == Qn::web_page)                  /* Qn::web_page is as flag combination. */
                return false;

            return accessController()->hasGlobalPermission(Qn::GlobalViewArchivePermission)
                || !navigator()->currentWidget()->resource()->flags().testFlag(Qn::live);   /* Show slider for local files. */
        };

    bool sliderVisible = calculateSliderVisible();

    if (m_inactive)
    {
        bool hovered = isHovered();
        setSliderVisible(sliderVisible && hovered, animate);
        setTreeVisible(hovered, animate);
        setTitleVisible(hovered, animate);
        setNotificationsVisible(hovered, animate);
    }
    else
    {
        setSliderVisible(sliderVisible, false);
        setTreeVisible(true, animate);
        setTitleVisible(true, animate);
        setNotificationsVisible(true, animate);
    }

    updateCalendarVisibility(animate);
}

QMargins QnWorkbenchUi::calculateViewportMargins(qreal treeX, qreal treeW, qreal titleY, qreal titleH, qreal sliderY, qreal notificationsX)
{
    QMargins result(
        isTreePinned() ? std::floor(qMax(0.0, treeX + treeW)) : 0.0,
        std::floor(qMax(0.0, titleY + titleH)),
        m_notificationsItem ? std::floor(qMax(0.0, isNotificationsPinned() ? m_controlsWidgetRect.right() - notificationsX : 0.0)) : 0.0,
        std::floor(qMax(0.0, m_controlsWidgetRect.bottom() - sliderY)));

    if (result.left() + result.right() >= m_controlsWidgetRect.width())
    {
        result.setLeft(0.0);
        result.setRight(0.0);
    }

    return result;

}

void QnWorkbenchUi::setFlags(Flags flags)
{
    if (flags == m_flags)
        return;

    m_flags = flags;

    updateActivityInstrumentState();
    updateViewportMargins();
}

bool QnWorkbenchUi::isTitleUsed() const
{
    return m_titleUsed;
}

void QnWorkbenchUi::updateViewportMargins()
{
    if (!m_flags.testFlag(AdjustMargins))
    {
        display()->setViewportMargins(QMargins(0, 0, 0, 0));
    }
    else
    {
        display()->setViewportMargins(calculateViewportMargins(
            m_tree.xAnimator ? (m_tree.xAnimator->isRunning() ? m_tree.xAnimator->targetValue().toReal() : m_tree.item->pos().x()) : 0.0,
            m_tree.item ? m_tree.item->size().width() : 0.0,
            m_titleYAnimator ? (m_titleYAnimator->isRunning() ? m_titleYAnimator->targetValue().toReal() : m_titleItem->pos().y()) : 0.0,
            m_titleItem ? m_titleItem->size().height() : 0.0,
            m_timeline.yAnimator->isRunning() ? m_timeline.yAnimator->targetValue().toReal() : m_timeline.item->pos().y(),
            m_notificationsItem ? m_notificationsXAnimator->isRunning() ? m_notificationsXAnimator->targetValue().toReal() : m_notificationsItem->pos().x() : 0.0
        ));
    }
}

void QnWorkbenchUi::updateActivityInstrumentState()
{
    bool zoomed = m_widgetByRole[Qn::ZoomedRole] != nullptr;
    m_controlsActivityInstrument->setEnabled(m_flags & (zoomed ? HideWhenZoomed : HideWhenNormal));
}

bool QnWorkbenchUi::isHovered() const
{
    return
        (m_timeline.opacityProcessor        && m_timeline.opacityProcessor->isHovered())
        || (m_tree.opacityProcessor          && m_tree.opacityProcessor->isHovered())
        || (m_titleOpacityProcessor         && m_titleOpacityProcessor->isHovered())
        || (m_notificationsOpacityProcessor && m_notificationsOpacityProcessor->isHovered())
        || (m_calendarOpacityProcessor      && m_calendarOpacityProcessor->isHovered())
        ;
}

QnWorkbenchUi::Panels QnWorkbenchUi::openedPanels() const
{
    return
        (isTreeOpened() ? TreePanel : NoPanel) |
        (isTitleOpened() ? TitlePanel : NoPanel) |
        (isSliderOpened() ? SliderPanel : NoPanel) |
        (isNotificationsOpened() ? NotificationsPanel : NoPanel);
}

void QnWorkbenchUi::setOpenedPanels(Panels panels, bool animate)
{
    ensureAnimationAllowed(animate);

    setTreeOpened(panels & TreePanel, animate);
    setTitleOpened(panels & TitlePanel, animate);
    setSliderOpened(panels & SliderPanel, animate);
    setNotificationsOpened(panels & NotificationsPanel, animate);
}

void QnWorkbenchUi::initGraphicsMessageBox()
{
    QGraphicsWidget *graphicsMessageBoxWidget = new QnUiElementsWidget();
    graphicsMessageBoxWidget->setAcceptedMouseButtons(0);
    display()->scene()->addItem(graphicsMessageBoxWidget);
    display()->setLayer(graphicsMessageBoxWidget, Qn::MessageBoxLayer);

    QGraphicsLinearLayout* messageBoxVLayout = new QGraphicsLinearLayout(Qt::Vertical);
    messageBoxVLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    messageBoxVLayout->setSpacing(0.0);

    QGraphicsLinearLayout* messageBoxHLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    messageBoxHLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    messageBoxHLayout->setSpacing(0.0);

    graphicsMessageBoxWidget->setLayout(messageBoxHLayout);

    messageBoxHLayout->addStretch();
    messageBoxHLayout->addItem(messageBoxVLayout);
    messageBoxHLayout->addStretch();

    messageBoxVLayout->addStretch();
    messageBoxVLayout->addItem(new QnGraphicsMessageBoxItem(graphicsMessageBoxWidget));
    messageBoxVLayout->addStretch();
}

void QnWorkbenchUi::tick(int deltaMSecs)
{
    if (!m_timeline.zoomingIn && !m_timeline.zoomingOut)
        return;

    if (!display()->scene())
        return;

    QnTimeSlider *slider = m_timeline.item->timeSlider();

    QPointF pos;
    if (slider->windowStart() <= slider->sliderPosition() && slider->sliderPosition() <= slider->windowEnd())
        pos = slider->positionFromValue(slider->sliderPosition(), true);
    else
        pos = slider->rect().center();

    QGraphicsSceneWheelEvent event(QEvent::GraphicsSceneWheel);
    event.setDelta(360 * 8 * (m_timeline.zoomingIn ? deltaMSecs : -deltaMSecs) / 1000); /* 360 degrees per sec, x8 since delta is measured in in eighths (1/8s) of a degree. */
    event.setPos(pos);
    event.setScenePos(slider->mapToScene(pos));
    display()->scene()->sendEvent(slider, &event);
}

void QnWorkbenchUi::at_freespaceAction_triggered()
{
    NX_ASSERT(!qnRuntime->isActiveXMode(), Q_FUNC_INFO, "This function must not be called in ActiveX mode.");
    if (qnRuntime->isActiveXMode())
        return;

    QAction *fullScreenAction = action(QnActions::EffectiveMaximizeAction);

    bool isFullscreen = fullScreenAction->isChecked();

    if (!m_inFreespace)
        m_inFreespace = isFullscreen && !isTreeOpened() && !isTitleOpened() && !isNotificationsOpened() && !isSliderOpened();

    if (!m_inFreespace)
    {
        storeSettings();

        if (!isFullscreen)
            fullScreenAction->setChecked(true);

        setTreeOpened(false, isFullscreen);
        setTitleOpened(false, isFullscreen);
        setSliderOpened(false, isFullscreen);
        setNotificationsOpened(false, isFullscreen);

        updateViewportMargins(); /* This one is needed here so that fit-in-view operates on correct margins. */ // TODO: #Elric change code so that this call is not needed.
        action(QnActions::FitInViewAction)->trigger();

        m_inFreespace = true;
    }
    else
    {
        QnPaneSettingsMap settings = qnSettings->paneSettings();
        setTreeOpened(settings[Qn::WorkbenchPane::Tree].state == Qn::PaneState::Opened, isFullscreen);
        setTitleOpened(settings[Qn::WorkbenchPane::Title].state == Qn::PaneState::Opened, isFullscreen);
        setSliderOpened(settings[Qn::WorkbenchPane::Navigation].state == Qn::PaneState::Opened, isFullscreen);
        setNotificationsOpened(settings[Qn::WorkbenchPane::Notifications].state == Qn::PaneState::Opened, isFullscreen);

        updateViewportMargins(); /* This one is needed here so that fit-in-view operates on correct margins. */ // TODO: #Elric change code so that this call is not needed.
        action(QnActions::FitInViewAction)->trigger();

        m_inFreespace = false;
    }
}

void QnWorkbenchUi::at_activityStopped()
{
    m_inactive = true;

    updateControlsVisibility(true);

    foreach(QnResourceWidget *widget, display()->widgets())
    {
        widget->setOption(QnResourceWidget::ActivityPresence, false);
        if (!(widget->options() & QnResourceWidget::DisplayInfo))
            widget->setOverlayVisible(false);
    }
    updateCursor();
}

void QnWorkbenchUi::at_activityStarted()
{
    m_inactive = false;

    updateControlsVisibility(true);

    foreach(QnResourceWidget *widget, display()->widgets())
    {
        widget->setOption(QnResourceWidget::ActivityPresence, true);
        if (widget->isInfoVisible()) // TODO: #Elric wrong place?
            widget->setOverlayVisible(true);
    }
    updateCursor();
}

void QnWorkbenchUi::at_display_widgetChanged(Qn::ItemRole role)
{
    //QnResourceWidget *oldWidget = m_widgetByRole[role];
    bool alreadyZoomed = m_widgetByRole[role] != nullptr;

    QnResourceWidget *newWidget = display()->widget(role);
    m_widgetByRole[role] = newWidget;

    /* Update activity listener instrument. */
    if (role == Qn::ZoomedRole)
    {
        updateActivityInstrumentState();
        updateViewportMargins();

        if (newWidget)
        {
            if (!alreadyZoomed)
                m_unzoomedOpenedPanels = openedPanels();
            setOpenedPanels(NoPanel, true);
        }
        else
        {
            /* User may have opened some panels while zoomed,
             * we want to leave them opened even if they were closed before. */
            setOpenedPanels(m_unzoomedOpenedPanels | openedPanels() | SliderPanel, true);

            /* Viewport margins have changed, force fit-in-view. */
            display()->fitInView();
        }

        m_timeline.showWidget->setVisible(newWidget);
    }

    if (qnRuntime->isVideoWallMode())
    {
        switch (role)
        {
            case Qn::ZoomedRole:
            case Qn::RaisedRole:
                if (newWidget)
                    updateControlsVisibility(true);
                else
                    setSliderVisible(false, true);
                break;

            default:
                break;
        }
    }
}

void QnWorkbenchUi::ensureAnimationAllowed(bool &animate)
{
    if (animate && (qnSettings->lightMode() & Qn::LightModeNoAnimation))
        animate = false;
}

#pragma region ControlsWidget

void QnWorkbenchUi::at_controlsWidget_geometryChanged()
{
    QGraphicsWidget *controlsWidget = m_controlsWidget;
    QRectF rect = controlsWidget->rect();
    if (qFuzzyEquals(m_controlsWidgetRect, rect))
        return;
    QRectF oldRect = m_controlsWidgetRect;
    m_controlsWidgetRect = rect;

    /* We lay everything out manually. */

    m_timeline.item->setGeometry(QRectF(
        0.0,
        m_timeline.item->pos().y() - oldRect.height() + rect.height(),
        rect.width(),
        m_timeline.item->size().height()));

    if (m_titleItem)
    {
        m_titleItem->setGeometry(QRectF(
            0.0,
            m_titleItem->pos().y(),
            rect.width(),
            m_titleItem->size().height()));
    }

    if (m_notificationsItem)
    {
        if (m_notificationsXAnimator->isRunning())
            m_notificationsXAnimator->stop();
        m_notificationsItem->setX(rect.right() + (isNotificationsOpened() ? -m_notificationsItem->size().width() : 1.0 /* Just in case. */));
    }

    updateTreeGeometry();
    updateNotificationsGeometry();
    updateFpsGeometry();
}

void QnWorkbenchUi::createControlsWidget()
{
    m_controlsWidget = new QnUiElementsWidget();
    m_controlsWidget->setAcceptedMouseButtons(0);
    display()->scene()->addItem(m_controlsWidget);
    display()->setLayer(m_controlsWidget, Qn::UiLayer);

    QnSingleEventSignalizer *deactivationSignalizer = new QnSingleEventSignalizer(this);
    deactivationSignalizer->setEventType(QEvent::WindowDeactivate);
    m_controlsWidget->installEventFilter(deactivationSignalizer);

    /* Reactivate controls widget if it is deactivated. */
    connect(deactivationSignalizer, &QnAbstractEventSignalizer::activated, this, [this]() { display()->scene()->setActiveWindow(m_controlsWidget); });

    connect(m_controlsWidget, &QGraphicsWidget::geometryChanged, this, &QnWorkbenchUi::at_controlsWidget_geometryChanged);
}

#pragma endregion Main controls widget methods

#pragma region TreeWidget

void QnWorkbenchUi::setTreeShowButtonUsed(bool used)
{
    m_tree.showButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void QnWorkbenchUi::setTreeVisible(bool visible, bool animate)
{
    ensureAnimationAllowed(animate);

    if (!m_tree.item)
        return;

    bool changed = m_treeVisible != visible;

    m_treeVisible = visible;

    updateTreeOpacity(animate);
    if (changed)
        updateTreeGeometry();
}

void QnWorkbenchUi::setTreeOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate)
{
    ensureAnimationAllowed(animate);

    if (animate)
    {
        m_tree.opacityAnimatorGroup->pause();
        opacityAnimator(m_tree.item)->setTargetValue(foregroundOpacity);
        opacityAnimator(m_tree.pinButton)->setTargetValue(foregroundOpacity);
        opacityAnimator(m_tree.backgroundItem)->setTargetValue(backgroundOpacity);
        opacityAnimator(m_tree.showButton)->setTargetValue(backgroundOpacity);
        m_tree.opacityAnimatorGroup->start();
    }
    else
    {
        m_tree.opacityAnimatorGroup->stop();
        m_tree.item->setOpacity(foregroundOpacity);
        m_tree.pinButton->setOpacity(foregroundOpacity);
        m_tree.backgroundItem->setOpacity(backgroundOpacity);
        m_tree.showButton->setOpacity(backgroundOpacity);
    }

    m_tree.resizerWidget->setVisible(!qFuzzyIsNull(foregroundOpacity));
}

void QnWorkbenchUi::updateTreeOpacity(bool animate)
{
    const qreal opacity = m_treeVisible ? kOpaque : kHidden;
    setTreeOpacity(opacity, opacity, animate);
}

bool QnWorkbenchUi::isTreePinned() const
{
    return action(QnActions::PinTreeAction)->isChecked();
}

bool QnWorkbenchUi::isCalendarPinned() const
{
    return action(QnActions::PinCalendarAction)->isChecked();
}

bool QnWorkbenchUi::isCalendarOpened() const
{
    return action(QnActions::ToggleCalendarAction)->isChecked();
}

bool QnWorkbenchUi::isTreeVisible() const
{
    return m_treeVisible;
}

bool QnWorkbenchUi::isSliderVisible() const
{
    return m_timeline.visible;
}

bool QnWorkbenchUi::isTitleVisible() const
{
    return m_titleVisible;
}

bool QnWorkbenchUi::isNotificationsVisible() const
{
    return m_notificationsVisible;
}

bool QnWorkbenchUi::isCalendarVisible() const
{
    return m_calendarVisible;
}

bool QnWorkbenchUi::isTreeOpened() const
{
    return action(QnActions::ToggleTreeAction)->isChecked();
}

void QnWorkbenchUi::setTreeOpened(bool opened, bool animate)
{
    ensureAnimationAllowed(animate);

    if (!m_tree.item)
        return;

    m_inFreespace &= !opened;

    m_tree.showingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(QnActions::ToggleTreeAction)->setChecked(opened);

    m_tree.setOpened(opened, animate);
}

QRectF QnWorkbenchUi::updatedTreeGeometry(const QRectF &treeGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry)
{
    QPointF pos(
        treeGeometry.x(),
        ((!m_titleVisible || !m_titleUsed) && m_treeVisible) ? 0.0 : qMax(titleGeometry.bottom(), 0.0));

    QSizeF size(
        treeGeometry.width(),
        ((!m_timeline.visible && m_treeVisible)
            ? m_controlsWidgetRect.bottom()
            : qMin(sliderGeometry.y(), m_controlsWidgetRect.bottom())) - pos.y());

    return QRectF(pos, size);
}

void QnWorkbenchUi::updateTreeGeometry()
{
    if (!m_tree.item)
        return;

    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedTreeGeometry(m_tree.item->geometry(), m_titleItem->geometry(), m_timeline.item->geometry());
    m_tree.item->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_tree.item->setPos(geometry.topLeft());

    /* Whether actual size change should be deferred. */
    bool defer = false;

    /* Calculate slider target position. */
    QPointF sliderPos;
    if (!m_timeline.visible && m_treeVisible)
    {
        sliderPos = QPointF(m_timeline.item->pos().x(), m_controlsWidgetRect.bottom());
    }
    else if (m_timeline.yAnimator->isRunning())
    {
        sliderPos = QPointF(m_timeline.item->pos().x(), m_timeline.yAnimator->targetValue().toReal());
        defer |= !qFuzzyEquals(sliderPos, m_timeline.item->pos()); /* If animation is running, then geometry sync should be deferred. */
    }
    else
    {
        sliderPos = m_timeline.item->pos();
    }

    /* Calculate title target position. */
    QPointF titlePos;
    if ((!m_titleVisible || !m_titleUsed) && m_treeVisible)
    {
        titlePos = QPointF(m_titleItem->pos().x(), -m_titleItem->size().height());
    }
    else if (m_titleYAnimator->isRunning())
    {
        titlePos = QPointF(m_titleItem->pos().x(), m_titleYAnimator->targetValue().toReal());
        defer |= !qFuzzyEquals(titlePos, m_titleItem->pos());
    }
    else
    {
        titlePos = m_titleItem->pos();
    }

    /* Calculate target geometry. */
    geometry = updatedTreeGeometry(m_tree.item->geometry(), QRectF(titlePos, m_titleItem->size()), QRectF(sliderPos, m_timeline.item->size()));
    if (qFuzzyEquals(geometry, m_tree.item->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if (defer && geometry.height() < m_tree.item->size().height())
        return;

    m_tree.item->resize(geometry.size());
}

void QnWorkbenchUi::updateTreeResizerGeometry()
{
    if (m_tree.updateResizerGeometryLater)
    {
        QTimer::singleShot(1, this, &QnWorkbenchUi::updateTreeResizerGeometry);
        return;
    }

    QRectF treeRect = m_tree.item->rect();

    QRectF treeResizerGeometry = QRectF(
        m_controlsWidget->mapFromItem(m_tree.item, treeRect.topRight()),
        m_controlsWidget->mapFromItem(m_tree.item, treeRect.bottomRight()));

    treeResizerGeometry.moveTo(treeResizerGeometry.topRight());
    treeResizerGeometry.setWidth(8);

    if (!qFuzzyEquals(treeResizerGeometry, m_tree.resizerWidget->geometry()))
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_tree.updateResizerGeometryLater, true);

        m_tree.resizerWidget->setGeometry(treeResizerGeometry);

        /* This one is needed here as we're in a handler and thus geometry change doesn't adjust position =(. */
        m_tree.resizerWidget->setPos(treeResizerGeometry.topLeft());  // TODO: #Elric remove this ugly hack.
    }
}

void QnWorkbenchUi::at_treeWidget_activated(const QnResourcePtr &resource)
{
    // user resources cannot be dropped on the scene
    if (!resource || resource.dynamicCast<QnUserResource>())
        return;

    menu()->trigger(QnActions::DropResourcesAction, resource);
}

void QnWorkbenchUi::at_treeItem_paintGeometryChanged()
{
    QRectF paintGeometry = m_tree.item->paintGeometry();

    /* Don't hide tree item here. It will repaint itself when shown, which will
     * degrade performance. */

    m_tree.backgroundItem->setGeometry(paintGeometry);

    m_tree.showButton->setPos(QPointF(
        qMax(m_controlsWidgetRect.left(), paintGeometry.right()),
        (m_controlsWidgetRect.top() + m_controlsWidgetRect.bottom() - m_tree.showButton->size().height()) / 2.0));

    m_tree.pinButton->setPos(QPointF(
        paintGeometry.right() - m_tree.pinButton->size().width() - 1.0,
        paintGeometry.top() + 1.0));

    updateTreeResizerGeometry();
    updateViewportMargins();
}

void QnWorkbenchUi::at_treeResizerWidget_geometryChanged()
{
    if (m_tree.ignoreResizerGeometryChanges)
        return;

    QRectF resizerGeometry = m_tree.resizerWidget->geometry();
    if (!resizerGeometry.isValid())
    {
        updateTreeResizerGeometry();
        return;
    }

    QRectF treeGeometry = m_tree.item->geometry();

    qreal targetWidth = m_tree.resizerWidget->geometry().left() - treeGeometry.left();
    qreal minWidth = m_tree.item->effectiveSizeHint(Qt::MinimumSize).width();

    //TODO #vkutin Think how to do it differently.
    // At application startup m_controlsWidget has default (not maximized) size, so we cannot use its width here.
    qreal maxWidth = mainWindow()->width() / 2;

    targetWidth = qBound(minWidth, targetWidth, maxWidth);

    if (!qFuzzyCompare(treeGeometry.width(), targetWidth))
    {
        treeGeometry.setWidth(targetWidth);
        treeGeometry.setLeft(0);

        QN_SCOPED_VALUE_ROLLBACK(&m_tree.ignoreResizerGeometryChanges, true);
        m_tree.widget->resize(targetWidth, m_tree.widget->height());
        m_tree.item->setPaintGeometry(treeGeometry);
    }

    updateTreeResizerGeometry();
}

void QnWorkbenchUi::at_treeShowingProcessor_hoverEntered()
{
    if (!isTreePinned() && !isTreeOpened())
    {
        setTreeOpened(true);

        /* So that the click that may follow won't hide it. */
        setTreeShowButtonUsed(false);
        QTimer::singleShot(kButtonInactivityTimeoutMs, this, [this]() { setTreeShowButtonUsed(true); });
    }

    m_tree.hidingProcessor->forceHoverEnter();
    m_tree.opacityProcessor->forceHoverEnter();
}

void QnWorkbenchUi::at_pinTreeAction_toggled(bool checked)
{
    if (checked)
        setTreeOpened(true);

    updateViewportMargins();
}

void QnWorkbenchUi::createTreeWidget(const QnPaneSettings& settings)
{
    m_tree.initialize(
        m_controlsWidget,
        m_instrumentManager->animationTimer(),
        settings, context());

    connect(m_tree.hidingProcessor, &HoverFocusProcessor::hoverFocusLeft, this,
        [this]
        {
            /* Do not auto-hide tree if we have opened context menu. */
            if (!isTreePinned() && isTreeOpened() && !menu()->isMenuVisible())
                setTreeOpened(false);
        });

    connect(menu(), &QnActionManager::menuAboutToHide, m_tree.hidingProcessor,
        &HoverFocusProcessor::forceFocusLeave);

    connect(m_tree.widget, &QnResourceBrowserWidget::selectionChanged,
        action(QnActions::SelectionChangeAction), &QAction::trigger);
    connect(m_tree.widget, &QnResourceBrowserWidget::activated, this,
        &QnWorkbenchUi::at_treeWidget_activated);
    connect(m_tree.opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &QnWorkbenchUi::updateTreeOpacityAnimated);
    connect(m_tree.opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &QnWorkbenchUi::updateTreeOpacityAnimated);
    connect(m_tree.opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_tree.opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_tree.showingProcessor, &HoverFocusProcessor::hoverEntered, this,
        &QnWorkbenchUi::at_treeShowingProcessor_hoverEntered);
    connect(m_tree.item, &QnMaskedProxyWidget::paintRectChanged, this,
        &QnWorkbenchUi::at_treeItem_paintGeometryChanged);
    connect(m_tree.item, &QGraphicsWidget::geometryChanged, this,
        &QnWorkbenchUi::at_treeItem_paintGeometryChanged);
    connect(m_tree.resizerWidget, &QGraphicsWidget::geometryChanged, this,
        &QnWorkbenchUi::at_treeResizerWidget_geometryChanged, Qt::QueuedConnection);

    connect(action(QnActions::ToggleTreeAction), &QAction::toggled, this,
        [this](bool checked)
        {
            if (!m_ignoreClickEvent)
                setTreeOpened(checked);
        });

    connect(action(QnActions::PinTreeAction), &QAction::toggled, this,
            &QnWorkbenchUi::at_pinTreeAction_toggled);
    connect(action(QnActions::PinNotificationsAction), &QAction::toggled, this,
        &QnWorkbenchUi::at_pinNotificationsAction_toggled);

    /* Create a shadow: */
    new QnEdgeShadowWidget(m_tree.item, Qt::RightEdge, NxUi::kShadowThickness);
}

#pragma endregion Tree widget methods

#pragma region TitleWidget

void QnWorkbenchUi::setTitleUsed(bool used)
{
    if (!m_titleItem)
        return;

    m_titleItem->setVisible(used);
    m_titleShowButton->setVisible(used);

    if (used)
    {
        m_titleUsed = used;
        setTitleOpened(isTitleOpened(), false);
        at_titleItem_geometryChanged();
    }
    else
    {
        m_titleItem->setPos(0.0, -m_titleItem->size().height() - 1.0);
        m_titleUsed = used;
    }
}

void QnWorkbenchUi::setTitleOpened(bool opened, bool animate)
{
    ensureAnimationAllowed(animate);

    m_inFreespace &= !opened;

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(QnActions::ToggleTitleBarAction)->setChecked(opened);

    if (!m_titleUsed)
        return;

    qreal newY = opened ? 0.0 : -m_titleItem->size().height() - 1.0;
    if (animate)
    {
        m_titleYAnimator->animateTo(newY);
    }
    else
    {
        m_titleYAnimator->stop();
        m_titleItem->setY(newY);
    }
}

void QnWorkbenchUi::setTitleVisible(bool visible, bool animate)
{
    ensureAnimationAllowed(animate);

    bool changed = m_titleVisible != visible;

    m_titleVisible = visible;

    updateTitleOpacity(animate);
    if (changed)
    {
        updateTreeGeometry();
        updateNotificationsGeometry();
    }
}

bool QnWorkbenchUi::isTitleOpened() const
{
    return action(QnActions::ToggleTitleBarAction)->isChecked();
}

void QnWorkbenchUi::setTitleOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate)
{
    ensureAnimationAllowed(animate);

    if (!m_titleItem)
        return;

    if (animate)
    {
        m_titleOpacityAnimatorGroup->pause();
        opacityAnimator(m_titleItem)->setTargetValue(foregroundOpacity);
        opacityAnimator(m_titleShowButton)->setTargetValue(backgroundOpacity);
        m_titleOpacityAnimatorGroup->start();
    }
    else
    {
        m_titleOpacityAnimatorGroup->stop();
        m_titleItem->setOpacity(foregroundOpacity);
        m_titleShowButton->setOpacity(backgroundOpacity);
    }
}

void QnWorkbenchUi::updateTitleOpacity(bool animate)
{
    const qreal opacity = m_titleVisible ? kOpaque : kHidden;
    setTitleOpacity(opacity, opacity, animate);
}

void QnWorkbenchUi::at_titleItem_geometryChanged()
{
    if (!m_titleUsed)
        return;

    updateTreeGeometry();
    updateNotificationsGeometry();
    updateFpsGeometry();

    QRectF geometry = m_titleItem->geometry();

    m_titleShowButton->setPos(QPointF(
        (geometry.left() + geometry.right() - m_titleShowButton->size().height()) / 2,
        qMax(m_controlsWidget->rect().top(), geometry.bottom())));
}

void QnWorkbenchUi::createTitleWidget(const QnPaneSettings& settings)
{
    m_titleItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_titleItem->setWidget(new QnMainWindowTitleBarWidget(nullptr, context()));
    m_titleItem->setPos(0.0, 0.0);
    m_titleItem->setZValue(10.0);

    const auto toggleTitleBarAction = action(QnActions::ToggleTitleBarAction);
    m_titleShowButton = NxUi::newShowHideButton(m_controlsWidget, context(), toggleTitleBarAction);
    {
        QTransform transform;
        transform.rotate(-90);
        transform.scale(-1, 1);
        m_titleShowButton->setTransform(transform);
    }
    m_titleShowButton->setFocusProxy(m_titleItem);

    m_titleOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_titleOpacityProcessor->addTargetItem(m_titleItem);
    m_titleOpacityProcessor->addTargetItem(m_titleShowButton);

    m_titleYAnimator = new VariantAnimator(this);
    m_titleYAnimator->setTimer(m_instrumentManager->animationTimer());
    m_titleYAnimator->setTargetObject(m_titleItem);
    m_titleYAnimator->setAccessor(new PropertyAccessor("y"));
    //m_titleYAnimator->setSpeed(m_titleItem->size().height() * 2.0); // TODO: #Elric why height is zero here?
    m_titleYAnimator->setSpeed(32.0 * 2.0);
    m_titleYAnimator->setTimeLimit(500);

    m_titleOpacityAnimatorGroup = new AnimatorGroup(this);
    m_titleOpacityAnimatorGroup->setTimer(m_instrumentManager->animationTimer());
    m_titleOpacityAnimatorGroup->addAnimator(opacityAnimator(m_titleItem));
    m_titleOpacityAnimatorGroup->addAnimator(opacityAnimator(m_titleShowButton));

    connect(m_titleOpacityProcessor, &HoverFocusProcessor::hoverEntered, this, &QnWorkbenchUi::updateTitleOpacityAnimated);
    connect(m_titleOpacityProcessor, &HoverFocusProcessor::hoverLeft, this, &QnWorkbenchUi::updateTitleOpacityAnimated);
    connect(m_titleOpacityProcessor, &HoverFocusProcessor::hoverEntered, this, &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_titleOpacityProcessor, &HoverFocusProcessor::hoverLeft, this, &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_titleItem, &QGraphicsWidget::geometryChanged, this, &QnWorkbenchUi::at_titleItem_geometryChanged);
    connect(action(QnActions::ToggleTitleBarAction), &QAction::toggled, this, [this](bool checked) { if (!m_ignoreClickEvent) setTitleOpened(checked); });

    toggleTitleBarAction->setChecked(settings.state == Qn::PaneState::Opened);

    /* Create a shadow: */
    new QnEdgeShadowWidget(m_titleItem, Qt::BottomEdge, NxUi::kShadowThickness);
}

#pragma endregion Title methods

#pragma region NotificationsWidget

bool QnWorkbenchUi::isNotificationsOpened() const
{
    return action(QnActions::ToggleNotificationsAction)->isChecked();
}

bool QnWorkbenchUi::isNotificationsPinned() const
{
    return action(QnActions::PinNotificationsAction)->isChecked();
}

void QnWorkbenchUi::setNotificationsOpened(bool opened, bool animate)
{
    if (!m_notificationsItem)
        return;

    ensureAnimationAllowed(animate);

    m_inFreespace &= !opened;

    m_notificationsShowingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    qreal newX = m_controlsWidgetRect.right() + (opened ? -m_notificationsItem->size().width() : 1.0 /* Just in case. */);
    if (animate)
    {
        m_notificationsXAnimator->setSpeed(qMax(1.0, m_notificationsItem->size().width() * 2.0));
        m_notificationsXAnimator->animateTo(newX);
    }
    else
    {
        m_notificationsXAnimator->stop();
        m_notificationsItem->setX(newX);
    }

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    m_notificationsShowButton->setChecked(opened);

    action(QnActions::ToggleNotificationsAction)->setChecked(opened);
}

void QnWorkbenchUi::setNotificationsShowButtonUsed(bool used)
{
    m_notificationsShowButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void QnWorkbenchUi::setNotificationsVisible(bool visible, bool animate)
{
    ensureAnimationAllowed(animate);

    if (!m_notificationsItem)
        return;

    bool changed = m_notificationsVisible != visible;

    m_notificationsVisible = visible;

    updateNotificationsOpacity(animate);
    if (changed)
        updateNotificationsGeometry();
}

void QnWorkbenchUi::setNotificationsOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate)
{
    ensureAnimationAllowed(animate);

    if (!m_notificationsItem)
        return;

    if (animate)
    {
        m_notificationsOpacityAnimatorGroup->pause();
        opacityAnimator(m_notificationsItem)->setTargetValue(foregroundOpacity);
        opacityAnimator(m_notificationsPinButton)->setTargetValue(foregroundOpacity);
        opacityAnimator(m_notificationsBackgroundItem)->setTargetValue(backgroundOpacity);
        opacityAnimator(m_notificationsShowButton)->setTargetValue(backgroundOpacity);
        m_notificationsOpacityAnimatorGroup->start();
    }
    else
    {
        m_notificationsOpacityAnimatorGroup->stop();
        m_notificationsItem->setOpacity(foregroundOpacity);
        m_notificationsPinButton->setOpacity(foregroundOpacity);
        m_notificationsBackgroundItem->setOpacity(backgroundOpacity);
        m_notificationsShowButton->setOpacity(backgroundOpacity);
    }
}

void QnWorkbenchUi::updateNotificationsOpacity(bool animate)
{
    const qreal opacity = m_notificationsVisible ? kOpaque : kHidden;
    setNotificationsOpacity(opacity, opacity, animate);
}

QRectF QnWorkbenchUi::updatedNotificationsGeometry(const QRectF &notificationsGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry)
{
    QPointF pos(
        notificationsGeometry.x(),
        ((!m_titleVisible || !m_titleUsed) && m_notificationsVisible) ? 0.0 : qMax(titleGeometry.bottom(), 0.0));

    const qreal maxHeight = (m_timeline.visible ? sliderGeometry.y() : m_controlsWidgetRect.bottom()) - pos.y();

    QSizeF size(notificationsGeometry.width(), maxHeight);
    return QRectF(pos, size);
}

void QnWorkbenchUi::updateNotificationsGeometry()
{
    if (!m_notificationsItem)
        return;

    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedNotificationsGeometry(m_notificationsItem->geometry(), m_titleItem->geometry(), m_timeline.item->geometry());

    /* Always change position. */
    m_notificationsItem->setPos(geometry.topLeft());

    /* Whether actual size change should be deferred. */
    bool defer = false;

    /* Calculate slider target position. */
    QPointF sliderPos;
    if (!m_timeline.visible && m_notificationsVisible)
    {
        sliderPos = QPointF(m_timeline.item->pos().x(), m_controlsWidgetRect.bottom());
    }
    else if (m_timeline.yAnimator->isRunning())
    {
        sliderPos = QPointF(m_timeline.item->pos().x(), m_timeline.yAnimator->targetValue().toReal());
        defer |= !qFuzzyEquals(sliderPos, m_timeline.item->pos()); /* If animation is running, then geometry sync should be deferred. */
    }
    else
    {
        sliderPos = m_timeline.item->pos();
    }

    /* Calculate title target position. */
    QPointF titlePos;
    if ((!m_titleVisible || !m_titleUsed) && m_notificationsVisible)
    {
        titlePos = QPointF(m_titleItem->pos().x(), -m_titleItem->size().height());
    }
    else if (m_titleYAnimator->isRunning())
    {
        titlePos = QPointF(m_titleItem->pos().x(), m_titleYAnimator->targetValue().toReal());
        defer |= !qFuzzyEquals(titlePos, m_titleItem->pos());
    }
    else
    {
        titlePos = m_titleItem->pos();
    }

    /* Calculate target geometry. */
    geometry = updatedNotificationsGeometry(m_notificationsItem->geometry(),
        QRectF(titlePos, m_titleItem->size()),
        QRectF(sliderPos, m_timeline.item->size()));

    if (qFuzzyEquals(geometry, m_notificationsItem->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if (defer && geometry.height() < m_notificationsItem->size().height())
        return;

    m_notificationsItem->resize(geometry.size());

    /* All tooltips should fit to rect of maxHeight */
    QRectF tooltipsEnclosingRect(
        m_controlsWidgetRect.left(),
        m_notificationsItem->y(),
        m_controlsWidgetRect.width(),
        m_notificationsItem->geometry().height());
    m_notificationsItem->setToolTipsEnclosingRect(m_controlsWidget->mapRectToItem(m_notificationsItem, tooltipsEnclosingRect));
}

void QnWorkbenchUi::at_pinNotificationsAction_toggled(bool checked)
{
    if (checked)
        setNotificationsOpened(true);

    updateViewportMargins();
}

void QnWorkbenchUi::at_notificationsShowingProcessor_hoverEntered()
{
    if (!isNotificationsPinned() && !isNotificationsOpened())
    {
        setNotificationsOpened(true);

        /* So that the click that may follow won't hide it. */
        setNotificationsShowButtonUsed(false);
        QTimer::singleShot(kButtonInactivityTimeoutMs, this, [this]() { setNotificationsShowButtonUsed(true); });
    }

    m_notificationsHidingProcessor->forceHoverEnter();
    m_notificationsOpacityProcessor->forceHoverEnter();
}


void QnWorkbenchUi::at_notificationsItem_geometryChanged()
{
    QRectF headerGeometry = m_controlsWidget->mapRectFromItem(m_notificationsItem, m_notificationsItem->headerGeometry());
    QRectF backgroundGeometry = m_controlsWidget->mapRectFromItem(m_notificationsItem, m_notificationsItem->visibleGeometry());

    QRectF paintGeometry = m_notificationsItem->geometry();

    /* Don't hide notifications item here. It will repaint itself when shown, which will
     * degrade performance. */

    m_notificationsBackgroundItem->setGeometry(paintGeometry);
    m_notificationsShowButton->setPos(QPointF(
        qMin(m_controlsWidgetRect.right(), paintGeometry.left()),
        (m_controlsWidgetRect.top() + m_controlsWidgetRect.bottom() - m_notificationsShowButton->size().height()) / 2
    ));
    m_notificationsPinButton->setPos(headerGeometry.topLeft() + QPointF(1.0, 1.0));
    if (isNotificationsOpened())
        setNotificationsOpened(); //there is no check there but it will fix the X-coord animation

    updateViewportMargins();
    updateFpsGeometry();
}

void QnWorkbenchUi::createNotificationsWidget(const QnPaneSettings& settings)
{
    /* Notifications panel. */
    m_notificationsBackgroundItem = new QnControlBackgroundWidget(Qn::RightBorder, m_controlsWidget);
    m_notificationsBackgroundItem->setFrameBorders(Qt::LeftEdge);

    m_notificationsItem = new QnNotificationsCollectionWidget(m_controlsWidget, 0, context());
    m_notificationsItem->setProperty(Qn::NoHandScrollOver, true);
    setHelpTopic(m_notificationsItem, Qn::MainWindow_Notifications_Help);

    const auto toggleNotificationsAction = action(QnActions::ToggleNotificationsAction);
    const auto pinNotificationsAction = action(QnActions::PinNotificationsAction);
    m_notificationsPinButton = NxUi::newPinButton(m_controlsWidget, context(), pinNotificationsAction);
    m_notificationsPinButton->setFocusProxy(m_notificationsItem);

    QnBlinkingImageButtonWidget* blinker = new QnBlinkingImageButtonWidget(m_controlsWidget);
    context()->statisticsModule()->registerButton(lit("notifications_collection_widget_toggle"), blinker);

    m_notificationsShowButton = blinker;
    m_notificationsShowButton->setCheckable(true);
    m_notificationsShowButton->setIcon(qnSkin->icon("panel/slide_right.png", "panel/slide_left.png"));
    m_notificationsShowButton->setProperty(Qn::NoHandScrollOver, true);
    m_notificationsShowButton->setTransform(QTransform::fromScale(-1, 1));
    m_notificationsShowButton->setFocusProxy(m_notificationsItem);
    m_notificationsShowButton->stackBefore(m_notificationsItem);
    m_notificationsShowButton->setFixedSize(QnSkin::maximumSize(m_notificationsShowButton->icon()));

    setHelpTopic(m_notificationsShowButton, Qn::MainWindow_Pin_Help);
    m_notificationsItem->setBlinker(blinker);

    m_notificationsOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_notificationsOpacityProcessor->addTargetItem(m_notificationsItem);
    m_notificationsOpacityProcessor->addTargetItem(m_notificationsShowButton);

    m_notificationsHidingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_notificationsHidingProcessor->addTargetItem(m_notificationsItem);
    m_notificationsHidingProcessor->addTargetItem(m_notificationsShowButton);
    m_notificationsHidingProcessor->setHoverLeaveDelay(NxUi::kCloseControlsTimeoutMs);
    m_notificationsHidingProcessor->setFocusLeaveDelay(NxUi::kCloseControlsTimeoutMs);

    m_notificationsShowingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_notificationsShowingProcessor->addTargetItem(m_notificationsShowButton);
    m_notificationsShowingProcessor->setHoverEnterDelay(NxUi::kShowControlsTimeoutMs);

    m_notificationsXAnimator = new VariantAnimator(this);
    m_notificationsXAnimator->setTimer(m_instrumentManager->animationTimer());
    m_notificationsXAnimator->setTargetObject(m_notificationsItem);
    m_notificationsXAnimator->setAccessor(new PropertyAccessor("x"));
    m_notificationsXAnimator->setTimeLimit(500);

    m_notificationsOpacityAnimatorGroup = new AnimatorGroup(this);
    m_notificationsOpacityAnimatorGroup->setTimer(m_instrumentManager->animationTimer());
    m_notificationsOpacityAnimatorGroup->addAnimator(opacityAnimator(m_notificationsItem));
    m_notificationsOpacityAnimatorGroup->addAnimator(opacityAnimator(m_notificationsBackgroundItem)); /* Speed of 1.0 is OK here. */
    m_notificationsOpacityAnimatorGroup->addAnimator(opacityAnimator(m_notificationsShowButton));
    m_notificationsOpacityAnimatorGroup->addAnimator(opacityAnimator(m_notificationsPinButton));

    connect(m_notificationsShowButton, &QnImageButtonWidget::toggled, this,
        [this](bool checked)
        {
            if (!m_ignoreClickEvent)
                setNotificationsOpened(checked);
        });
    connect(m_notificationsOpacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &QnWorkbenchUi::updateNotificationsOpacityAnimated);
    connect(m_notificationsOpacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &QnWorkbenchUi::updateNotificationsOpacityAnimated);
    connect(m_notificationsOpacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_notificationsOpacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_notificationsHidingProcessor, &HoverFocusProcessor::hoverFocusLeft, this,
        [this]
        {
            if (!isNotificationsPinned())
                setNotificationsOpened(false);
        });
    connect(m_notificationsShowingProcessor, &HoverFocusProcessor::hoverEntered, this,
        &QnWorkbenchUi::at_notificationsShowingProcessor_hoverEntered);
    connect(m_notificationsItem, &QGraphicsWidget::geometryChanged, this,
        &QnWorkbenchUi::at_notificationsItem_geometryChanged);
    connect(m_notificationsItem, &QnNotificationsCollectionWidget::visibleSizeChanged, this,
        &QnWorkbenchUi::at_notificationsItem_geometryChanged);
    connect(m_notificationsItem, &QnNotificationsCollectionWidget::sizeHintChanged, this,
        &QnWorkbenchUi::updateNotificationsGeometry);

    toggleNotificationsAction->setChecked(settings.state == Qn::PaneState::Opened);
    pinNotificationsAction->setChecked(settings.state != Qn::PaneState::Unpinned);

    /* Create a shadow: */
    new QnEdgeShadowWidget(m_notificationsItem, Qt::LeftEdge, NxUi::kShadowThickness);
}

#pragma endregion Notifications widget methods

#pragma region CalendarWidget

void QnWorkbenchUi::setCalendarVisible(bool visible, bool animate)
{
    ensureAnimationAllowed(animate);

    bool changed = m_calendarVisible != visible;

    m_calendarVisible = visible;

    updateCalendarOpacity(animate);
    if (changed)
        updateNotificationsGeometry();
}

void QnWorkbenchUi::setCalendarOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(animate);

    if (animate)
    {
        m_calendarOpacityAnimatorGroup->pause();
        opacityAnimator(m_calendarItem)->setTargetValue(opacity);
        opacityAnimator(m_dayTimeItem)->setTargetValue(opacity);
        opacityAnimator(m_calendarPinButton)->setTargetValue(opacity);
        opacityAnimator(m_dayTimeMinimizeButton)->setTargetValue(opacity);
        m_calendarOpacityAnimatorGroup->start();
    }
    else
    {
        m_calendarOpacityAnimatorGroup->stop();
        m_calendarItem->setOpacity(opacity);
        m_dayTimeItem->setOpacity(opacity);
        m_calendarPinButton->setOpacity(opacity);
        m_dayTimeMinimizeButton->setOpacity(opacity);
    }
}

void QnWorkbenchUi::setCalendarOpened(bool opened, bool animate)
{
    if (!opened && isCalendarPinned() && action(QnActions::ToggleCalendarAction)->isChecked())
        return;

    ensureAnimationAllowed(animate);

    m_calendarShowingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    QSizeF newSize(m_calendarItem->size());
    if (!opened)
        newSize.setHeight(0.0);

    if (animate)
    {
        m_calendarSizeAnimator->animateTo(newSize);
    }
    else
    {
        m_calendarSizeAnimator->stop();
        m_calendarItem->setPaintSize(newSize);
    }

    action(QnActions::ToggleCalendarAction)->setChecked(opened);

    if (!opened)
        setDayTimeWidgetOpened(opened, animate);
}

void QnWorkbenchUi::setDayTimeWidgetOpened(bool opened, bool animate)
{
    ensureAnimationAllowed(animate);

    m_dayTimeOpened = opened;

    QSizeF newSize(m_dayTimeItem->size());
    if (!opened)
        newSize.setHeight(0.0);

    if (animate)
    {
        m_dayTimeSizeAnimator->animateTo(newSize);
    }
    else
    {
        m_dayTimeSizeAnimator->stop();
        m_dayTimeItem->setPaintSize(newSize);
    }
}

void QnWorkbenchUi::updateCalendarOpacity(bool animate)
{
    const qreal opacity = m_calendarVisible ? kOpaque : kHidden;
    setCalendarOpacity(opacity, animate);
}

void QnWorkbenchUi::updateCalendarVisibility(bool animate)
{
    ensureAnimationAllowed(animate);

    bool calendarEmpty = true;
    if (QnCalendarWidget* c = dynamic_cast<QnCalendarWidget *>(m_calendarItem->widget()))
        calendarEmpty = c->isEmpty(); /* Small hack. We have a signal that updates visibility if a calendar receive new data */

    bool calendarEnabled = !calendarEmpty && (navigator()->currentWidget() && navigator()->currentWidget()->resource()->flags() & Qn::utc);
    action(QnActions::ToggleCalendarAction)->setEnabled(calendarEnabled); // TODO: #GDM #Common does this belong here?

    bool calendarVisible = calendarEnabled && m_timeline.visible && isSliderOpened();
    setCalendarVisible(calendarVisible && (!m_inactive || isHovered()), animate);

    if (!calendarVisible)
        setCalendarOpened(false);
}

QRectF QnWorkbenchUi::updatedCalendarGeometry(const QRectF &sliderGeometry)
{
    QRectF geometry = m_calendarItem->paintGeometry();
    geometry.moveRight(m_controlsWidgetRect.right());
    geometry.moveBottom(sliderGeometry.top());
    return geometry;
}

void QnWorkbenchUi::updateCalendarGeometry()
{
    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedCalendarGeometry(m_timeline.item->geometry());
    m_calendarItem->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_calendarItem->setPos(geometry.topLeft());
}

QRectF QnWorkbenchUi::updatedDayTimeWidgetGeometry(const QRectF &sliderGeometry, const QRectF &calendarGeometry)
{
    QRectF geometry = m_dayTimeItem->paintGeometry();
    geometry.moveRight(sliderGeometry.right());
    geometry.moveBottom(calendarGeometry.top());
    return geometry;
}

void QnWorkbenchUi::updateDayTimeWidgetGeometry()
{
    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedDayTimeWidgetGeometry(m_timeline.item->geometry(), m_calendarItem->geometry());
    m_dayTimeItem->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_dayTimeItem->setPos(geometry.topLeft());
}

void QnWorkbenchUi::setCalendarShowButtonUsed(bool used)
{
    m_timeline.item->calendarButton()->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void QnWorkbenchUi::at_calendarShowingProcessor_hoverEntered()
{
    if (!isCalendarPinned() && !isCalendarOpened())
    {
        setCalendarOpened(true);

        /* So that the click that may follow won't hide it. */
        setCalendarShowButtonUsed(false);
        QTimer::singleShot(kButtonInactivityTimeoutMs, this, [this]() { setCalendarShowButtonUsed(true); });
    }

    m_calendarHidingProcessor->forceHoverEnter();
    m_calendarOpacityProcessor->forceHoverEnter();
}

void QnWorkbenchUi::at_calendarItem_paintGeometryChanged()
{
    const QRectF paintGeometry = m_calendarItem->paintGeometry();
    m_calendarPinButton->setPos(paintGeometry.topRight() + m_calendarPinOffset);
    m_calendarPinButton->setVisible(!paintGeometry.isEmpty());

    if (m_inCalendarGeometryUpdate)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_inCalendarGeometryUpdate, true);

    updateCalendarGeometry();
    updateDayTimeWidgetGeometry();
    updateNotificationsGeometry();
}

void QnWorkbenchUi::at_dayTimeItem_paintGeometryChanged()
{
    const QRectF paintGeomerty = m_dayTimeItem->paintGeometry();
    m_dayTimeMinimizeButton->setPos(paintGeomerty.topRight() + m_dayTimeOffset);
    m_dayTimeMinimizeButton->setVisible(paintGeomerty.height());

    if (m_inDayTimeGeometryUpdate)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_inDayTimeGeometryUpdate, true);

    updateDayTimeWidgetGeometry();
    updateNotificationsGeometry();
}

void QnWorkbenchUi::at_calendarWidget_dateClicked(const QDate &date)
{
    const bool sameDate = (m_dayTimeWidget->date() == date);
    m_dayTimeWidget->setDate(date);

    if (isCalendarOpened())
    {
        const bool shouldBeOpened = !sameDate || !m_dayTimeOpened;
        setDayTimeWidgetOpened(shouldBeOpened, true);
    }
}

void QnWorkbenchUi::createCalendarWidget(const QnPaneSettings& settings)
{
    QnCalendarWidget* calendarWidget = new QnCalendarWidget();
    setHelpTopic(calendarWidget, Qn::MainWindow_Calendar_Help);
    navigator()->setCalendar(calendarWidget);

    m_dayTimeWidget = new QnDayTimeWidget();
    setHelpTopic(m_dayTimeWidget, Qn::MainWindow_DayTimePicker_Help);
    navigator()->setDayTimeWidget(m_dayTimeWidget);

    m_calendarItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_calendarItem->setWidget(calendarWidget);
    calendarWidget->installEventFilter(m_calendarItem);
    m_calendarItem->resize(250, 192);

    m_calendarItem->setProperty(Qn::NoHandScrollOver, true);

    const auto pinCalendarAction = action(QnActions::PinCalendarAction);
    pinCalendarAction->setChecked(settings.state != Qn::PaneState::Unpinned);
    m_calendarPinButton = NxUi::newPinButton(m_controlsWidget, context(), pinCalendarAction, true);
    m_calendarPinButton->setFocusProxy(m_calendarItem);

    const auto toggleCalendarAction = action(QnActions::ToggleCalendarAction);
    toggleCalendarAction->setChecked(settings.state == Qn::PaneState::Opened);

    m_dayTimeItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_dayTimeItem->setWidget(m_dayTimeWidget);
    m_dayTimeWidget->installEventFilter(m_calendarItem);
    m_dayTimeItem->resize(250, 120);
    m_dayTimeItem->setProperty(Qn::NoHandScrollOver, true);
    m_dayTimeItem->stackBefore(m_calendarItem);

    m_dayTimeMinimizeButton = NxUi::newActionButton(m_controlsWidget, context(), action(QnActions::MinimizeDayTimeViewAction), Qn::Empty_Help);
    m_dayTimeMinimizeButton->setFocusProxy(m_dayTimeItem);

    m_calendarOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_calendarOpacityProcessor->addTargetItem(m_calendarItem);
    m_calendarOpacityProcessor->addTargetItem(m_dayTimeItem);
    m_calendarOpacityProcessor->addTargetItem(m_calendarPinButton);
    m_calendarOpacityProcessor->addTargetItem(m_dayTimeMinimizeButton);

    m_calendarHidingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_calendarHidingProcessor->addTargetItem(m_calendarItem);
    m_calendarHidingProcessor->addTargetItem(m_dayTimeItem);
    m_calendarHidingProcessor->addTargetItem(m_calendarPinButton);
    m_calendarHidingProcessor->setHoverLeaveDelay(NxUi::kCloseControlsTimeoutMs);
    m_calendarHidingProcessor->setFocusLeaveDelay(NxUi::kCloseControlsTimeoutMs);

    m_calendarShowingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_calendarShowingProcessor->setHoverEnterDelay(NxUi::kShowControlsTimeoutMs);

    m_calendarSizeAnimator = new VariantAnimator(this);
    m_calendarSizeAnimator->setTimer(m_instrumentManager->animationTimer());
    m_calendarSizeAnimator->setTargetObject(m_calendarItem);
    m_calendarSizeAnimator->setAccessor(new PropertyAccessor("paintSize"));
    m_calendarSizeAnimator->setSpeed(100.0 * 2.0);
    m_calendarSizeAnimator->setTimeLimit(500);

    m_dayTimeSizeAnimator = new VariantAnimator(this);
    m_dayTimeSizeAnimator->setTimer(m_instrumentManager->animationTimer());
    m_dayTimeSizeAnimator->setTargetObject(m_dayTimeItem);
    m_dayTimeSizeAnimator->setAccessor(new PropertyAccessor("paintSize"));
    m_dayTimeSizeAnimator->setSpeed(100.0 * 2.0);
    m_dayTimeSizeAnimator->setTimeLimit(500);

    m_calendarOpacityAnimatorGroup = new AnimatorGroup(this);
    m_calendarOpacityAnimatorGroup->setTimer(m_instrumentManager->animationTimer());
    m_calendarOpacityAnimatorGroup->addAnimator(opacityAnimator(m_calendarItem));
    m_calendarOpacityAnimatorGroup->addAnimator(opacityAnimator(m_dayTimeItem));
    m_calendarOpacityAnimatorGroup->addAnimator(opacityAnimator(m_calendarPinButton));
    m_calendarOpacityAnimatorGroup->addAnimator(opacityAnimator(m_dayTimeMinimizeButton));

    connect(calendarWidget, &QnCalendarWidget::emptyChanged, this,
        &QnWorkbenchUi::updateCalendarVisibilityAnimated);
    connect(calendarWidget, &QnCalendarWidget::dateClicked, this,
        &QnWorkbenchUi::at_calendarWidget_dateClicked);
    connect(m_dayTimeItem, &QnMaskedProxyWidget::paintRectChanged, this,
        &QnWorkbenchUi::at_dayTimeItem_paintGeometryChanged);
    connect(m_dayTimeItem, &QGraphicsWidget::geometryChanged, this,
        &QnWorkbenchUi::at_dayTimeItem_paintGeometryChanged);
    connect(m_calendarOpacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &QnWorkbenchUi::updateCalendarOpacityAnimated);
    connect(m_calendarOpacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &QnWorkbenchUi::updateCalendarOpacityAnimated);
    connect(m_calendarOpacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_calendarOpacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_calendarHidingProcessor, &HoverFocusProcessor::hoverLeft, this,
        [this]
        {
            setCalendarOpened(false);
        });
    connect(m_calendarShowingProcessor, &HoverFocusProcessor::hoverEntered, this,
        &QnWorkbenchUi::at_calendarShowingProcessor_hoverEntered);
    connect(m_calendarItem, &QnMaskedProxyWidget::paintRectChanged, this,
        &QnWorkbenchUi::at_calendarItem_paintGeometryChanged);
    connect(m_calendarItem, &QGraphicsWidget::geometryChanged, this,
        &QnWorkbenchUi::at_calendarItem_paintGeometryChanged);
    connect(toggleCalendarAction, &QAction::toggled, this,
        [this](bool checked)
        {
            setCalendarOpened(checked);
        });
    connect(action(QnActions::MinimizeDayTimeViewAction), &QAction::triggered, this,
        [this]
        {
            setDayTimeWidgetOpened(false, true);
        });

    static const int kCellsCountOffset = 2;
    const int size = calendarWidget->headerHeight();
    m_calendarPinOffset = QPoint(-kCellsCountOffset * size, 0.0);
    m_dayTimeOffset = QPoint(-m_dayTimeWidget->headerHeight(), 0);
}

#pragma endregion Calendar and DayTime widget methods

#pragma region SliderWidget

void QnWorkbenchUi::setSliderShowButtonUsed(bool used)
{
    m_timeline.showButton->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

bool QnWorkbenchUi::isThumbnailsVisible() const
{
    qreal height = m_timeline.item->geometry().height();
    return height != 0.0 && !qFuzzyCompare(height, m_timeline.item->effectiveSizeHint(Qt::MinimumSize).height());
}

void QnWorkbenchUi::setThumbnailsVisible(bool visible)
{
    if (visible == isThumbnailsVisible())
        return;

    qreal sliderHeight = m_timeline.item->effectiveSizeHint(Qt::MinimumSize).height();
    if (!visible)
        m_timeline.lastThumbnailsHeight = m_timeline.item->geometry().height() - sliderHeight;
    else
        sliderHeight += m_timeline.lastThumbnailsHeight;

    QRectF geometry = m_timeline.item->geometry();
    geometry.setHeight(sliderHeight);
    m_timeline.item->setGeometry(geometry);
}

bool QnWorkbenchUi::isSliderOpened() const
{
    return m_timeline.opened;
}

bool QnWorkbenchUi::isSliderPinned() const
{
    /* Auto-hide slider when some item is zoomed, otherwise - don't. */
    return m_timeline.pinned ||
        m_widgetByRole[Qn::ZoomedRole] == nullptr;
}

void QnWorkbenchUi::setSliderOpened(bool opened, bool animate)
{
    if (qnRuntime->isVideoWallMode())
        opened = true;

    /* Do not hide pinned timeline. */
    if (m_timeline.pinned && !opened)
        return;

    m_timeline.opened = opened;

    ensureAnimationAllowed(animate);

    m_inFreespace &= !opened;

    qreal newY = m_controlsWidgetRect.bottom()
        + (opened ? -m_timeline.item->size().height() : 64.0 /* So that tooltips are not opened. */);
    if (animate)
    {
        /* Skip off-screen part. */
        if (opened && m_timeline.item->y() > m_controlsWidgetRect.bottom())
        {
            QSignalBlocker blocker(m_timeline.item);
            m_timeline.item->setY(m_controlsWidgetRect.bottom());
        }

        m_timeline.yAnimator->animateTo(newY);
    }
    else
    {
        m_timeline.yAnimator->stop();
        m_timeline.item->setY(newY);
    }

    updateCalendarVisibility(animate);

    m_timeline.resizerWidget->setEnabled(opened);
}

void QnWorkbenchUi::setSliderVisible(bool visible, bool animate)
{
    ensureAnimationAllowed(animate);

    bool changed = m_timeline.visible != visible;

    m_timeline.visible = visible;
    if (qnRuntime->isVideoWallMode())
    {
        if (visible)
            m_timeline.autoHideTimer->start(kVideoWallTimelineAutoHideTimeoutMs);
        else
            m_timeline.autoHideTimer->stop();
    }

    updateSliderOpacity(animate);
    if (changed)
    {
        updateTreeGeometry();
        updateNotificationsGeometry();
        updateCalendarVisibility(animate);
        m_timeline.item->setEnabled(m_timeline.visible); /* So that it doesn't handle mouse events while disappearing. */
    }
}

void QnWorkbenchUi::setSliderOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(animate);

    if (animate)
    {
        m_timeline.opacityAnimatorGroup->pause();
        opacityAnimator(m_timeline.item)->setTargetValue(opacity);
        opacityAnimator(m_timeline.showButton)->setTargetValue(opacity);
        m_timeline.opacityAnimatorGroup->start();
    }
    else
    {
        m_timeline.opacityAnimatorGroup->stop();
        m_timeline.item->setOpacity(opacity);
        m_timeline.showButton->setOpacity(opacity);
    }

    m_timeline.resizerWidget->setVisible(!qFuzzyIsNull(opacity));
}

void QnWorkbenchUi::setSliderZoomButtonsOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(animate);

    if (animate)
        opacityAnimator(m_timeline.zoomButtonsWidget)->animateTo(opacity);
    else
        m_timeline.zoomButtonsWidget->setOpacity(opacity);
}

void QnWorkbenchUi::updateSliderOpacity(bool animate)
{
    const qreal opacity = m_timeline.visible ? kOpaque : kHidden;
    setSliderOpacity(opacity, animate);

    bool isButtonOpaque = m_timeline.visible && m_timeline.opacityProcessor && m_timeline.opacityProcessor->isHovered();
    const qreal buttonsOpacity = isButtonOpaque ? kOpaque : kHidden;
    setSliderZoomButtonsOpacity(buttonsOpacity, animate);
}

void QnWorkbenchUi::updateSliderResizerGeometry()
{
    if (m_timeline.updateResizerGeometryLater)
    {
        QTimer::singleShot(1, this, &QnWorkbenchUi::updateSliderResizerGeometry);
        return;
    }

    QnTimeSlider *timeSlider = m_timeline.item->timeSlider();
    QRectF timeSliderRect = timeSlider->rect();

    QRectF sliderResizerGeometry = QRectF(
        m_controlsWidget->mapFromItem(timeSlider, timeSliderRect.topLeft()),
        m_controlsWidget->mapFromItem(timeSlider, timeSliderRect.topRight()));

    sliderResizerGeometry.moveTo(sliderResizerGeometry.topLeft() - QPointF(0, 8));
    sliderResizerGeometry.setHeight(16);

    if (!qFuzzyEquals(sliderResizerGeometry, m_timeline.resizerWidget->geometry()))
    {
        QN_SCOPED_VALUE_ROLLBACK(&m_timeline.updateResizerGeometryLater, true);

        m_timeline.resizerWidget->setGeometry(sliderResizerGeometry);

        /* This one is needed here as we're in a handler and thus geometry change doesn't adjust position =(. */
        m_timeline.resizerWidget->setPos(sliderResizerGeometry.topLeft());  // TODO: #Elric remove this ugly hack.
    }

}

void QnWorkbenchUi::updateSliderZoomButtonsGeometry()
{
    QPointF pos = m_timeline.item->timeSlider()->mapToItem(m_controlsWidget, m_timeline.item->timeSlider()->rect().topLeft());
    m_timeline.zoomButtonsWidget->setPos(pos);
}

void QnWorkbenchUi::at_sliderResizerWidget_wheelEvent(QObject *, QEvent *event)
{
    QGraphicsSceneWheelEvent *oldEvent = static_cast<QGraphicsSceneWheelEvent *>(event);
    QGraphicsSceneWheelEvent newEvent(QEvent::GraphicsSceneWheel);
    newEvent.setDelta(oldEvent->delta());
    newEvent.setPos(m_timeline.item->timeSlider()->mapFromItem(m_timeline.resizerWidget, oldEvent->pos()));
    newEvent.setScenePos(oldEvent->scenePos());
    display()->scene()->sendEvent(m_timeline.item->timeSlider(), &newEvent);
}

void QnWorkbenchUi::at_sliderItem_geometryChanged()
{
    setSliderOpened(isSliderOpened(), m_timeline.yAnimator->isRunning()); /* Re-adjust to screen sides. */

    updateTreeGeometry();
    updateNotificationsGeometry();

    updateViewportMargins();
    updateSliderResizerGeometry();
    updateCalendarGeometry();
    updateSliderZoomButtonsGeometry();
    updateDayTimeWidgetGeometry();

    QRectF geometry = m_timeline.item->geometry();

    /* Button is painted rotated, so we taking into account its height, not width. */
    QPointF showButtonPos(
        (geometry.left() + geometry.right() - m_timeline.showButton->geometry().height()) / 2,
        qMin(m_controlsWidgetRect.bottom(), geometry.top()));
    m_timeline.showButton->setPos(showButtonPos);

    static const int kShowWidgetHeight = 50;
    static const int kShowWidgetHiddenHeight = 12;

    const int showWidgetY = m_timeline.opened
        ? geometry.top() - kShowWidgetHeight
        : m_controlsWidgetRect.bottom() - kShowWidgetHiddenHeight;

    QRectF showWidgetGeometry(geometry);
    showWidgetGeometry.setTop(showWidgetY);
    showWidgetGeometry.setHeight(kShowWidgetHeight);
    m_timeline.showWidget->setGeometry(showWidgetGeometry);

    if (isThumbnailsVisible())
    {
        qreal sliderHeight = m_timeline.item->effectiveSizeHint(Qt::MinimumSize).height();
        m_timeline.lastThumbnailsHeight = m_timeline.item->geometry().height() - sliderHeight;
    }
}

void QnWorkbenchUi::at_sliderResizerWidget_geometryChanged()
{
    if (m_timeline.ignoreResizerGeometryChanges)
        return;

    QRectF sliderResizerGeometry = m_timeline.resizerWidget->geometry();
    if (!sliderResizerGeometry.isValid())
    {
        updateSliderResizerGeometry();
        return;
    }

    QRectF sliderGeometry = m_timeline.item->geometry();

    qreal targetHeight = sliderGeometry.bottom() - sliderResizerGeometry.center().y();
    qreal minHeight = m_timeline.item->effectiveSizeHint(Qt::MinimumSize).height();
    qreal jmpHeight = minHeight + 48.0;
    qreal maxHeight = minHeight + 196.0;

    if (targetHeight < (minHeight + jmpHeight) / 2)
        targetHeight = minHeight;
    else if (targetHeight < jmpHeight)
        targetHeight = jmpHeight;
    else if (targetHeight > maxHeight)
        targetHeight = maxHeight;

    if (!qFuzzyCompare(sliderGeometry.height(), targetHeight))
    {
        qreal sliderTop = sliderGeometry.top();
        sliderGeometry.setHeight(targetHeight);
        sliderGeometry.moveTop(sliderTop);

        QN_SCOPED_VALUE_ROLLBACK(&m_timeline.ignoreResizerGeometryChanges, true);
        m_timeline.item->setGeometry(sliderGeometry);
    }

    updateSliderResizerGeometry();

    action(QnActions::ToggleThumbnailsAction)->setChecked(isThumbnailsVisible());
}

void QnWorkbenchUi::createSliderWidget(const QnPaneSettings& settings)
{
    m_timeline.resizerWidget = new QnResizerWidget(Qt::Vertical, m_controlsWidget);
    m_timeline.resizerWidget->setProperty(Qn::NoHandScrollOver, true);

    m_timeline.item = new QnNavigationItem(m_controlsWidget);
    m_timeline.item->setProperty(Qn::NoHandScrollOver, true);
    m_timeline.item->timeSlider()->toolTipItem()->setProperty(Qn::NoHandScrollOver, true);
    m_timeline.item->speedSlider()->toolTipItem()->setProperty(Qn::NoHandScrollOver, true);
    m_timeline.item->volumeSlider()->toolTipItem()->setProperty(Qn::NoHandScrollOver, true);

    /*
    Calendar is created before navigation slider (alot of logic relies on that).
    Therefore we have to bind calendar showing/hiding processors to navigation
    pane button "CLND" here and not in createCalendarWidget()
    */
    m_calendarHidingProcessor->addTargetItem(m_timeline.item->calendarButton());
    // m_calendarShowingProcessor->addTargetItem(m_timeline.item->calendarButton()); // - show unpinned calendar on hovering its button without pressing

    const auto toggleSliderAction = action(QnActions::ToggleSliderAction);
    m_timeline.showButton = NxUi::newShowHideButton(m_controlsWidget, context(), toggleSliderAction);
    {
        QTransform transform;
        transform.rotate(-90);
        m_timeline.showButton->setTransform(transform);
    }
    m_timeline.showButton->setFocusProxy(m_timeline.item);

    m_timeline.showWidget = new GraphicsWidget(m_controlsWidget);
    m_timeline.showWidget->setProperty(Qn::NoHandScrollOver, true);
    m_timeline.showWidget->setFlag(QGraphicsItem::ItemHasNoContents, true);
    m_timeline.showWidget->setVisible(false);

    m_timeline.showingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_timeline.showingProcessor->addTargetItem(m_timeline.showButton);
    m_timeline.showingProcessor->addTargetItem(m_timeline.showWidget);
    m_timeline.showingProcessor->addTargetItem(m_timeline.resizerWidget);
    m_timeline.showingProcessor->setHoverEnterDelay(kShowTimelineTimeoutMs);

    m_timeline.hidingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_timeline.hidingProcessor->addTargetItem(m_timeline.item);
    m_timeline.hidingProcessor->addTargetItem(m_timeline.showButton);
    m_timeline.hidingProcessor->addTargetItem(m_timeline.resizerWidget);
    m_timeline.hidingProcessor->addTargetItem(m_timeline.showWidget);
    m_timeline.hidingProcessor->setHoverLeaveDelay(kCloseTimelineTimeoutMs);
    m_timeline.hidingProcessor->setFocusLeaveDelay(kCloseTimelineTimeoutMs);

    if (qnRuntime->isVideoWallMode())
    {
        m_timeline.showButton->setVisible(false);
        m_timeline.autoHideTimer = new QTimer(this);
        connect(m_timeline.autoHideTimer, &QTimer::timeout, this,
            [this]
            {
                setSliderVisible(false, true);
            });
    }

    QnImageButtonWidget *sliderZoomOutButton = new QnImageButtonWidget();
    sliderZoomOutButton->setIcon(qnSkin->icon("slider/buttons/zoom_out.png"));
    sliderZoomOutButton->setPreferredSize(19, 16);
    context()->statisticsModule()->registerButton(lit("slider_zoom_out"), sliderZoomOutButton);

    QnImageButtonWidget *sliderZoomInButton = new QnImageButtonWidget();
    sliderZoomInButton->setIcon(qnSkin->icon("slider/buttons/zoom_in.png"));
    sliderZoomInButton->setPreferredSize(19, 16);
    context()->statisticsModule()->registerButton(lit("slider_zoom_in"), sliderZoomInButton);

    QGraphicsLinearLayout *sliderZoomButtonsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    sliderZoomButtonsLayout->setSpacing(0.0);
    sliderZoomButtonsLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    sliderZoomButtonsLayout->addItem(sliderZoomOutButton);
    sliderZoomButtonsLayout->addItem(sliderZoomInButton);

    m_timeline.zoomButtonsWidget = new GraphicsWidget(m_controlsWidget);
    m_timeline.zoomButtonsWidget->setLayout(sliderZoomButtonsLayout);
    m_timeline.zoomButtonsWidget->setOpacity(0.0);

    m_timeline.zoomButtonsWidget->setVisible(navigator()->hasArchive());
    connect(navigator(), &QnWorkbenchNavigator::hasArchiveChanged, this,
        [this]()
        {
            m_timeline.zoomButtonsWidget->setVisible(navigator()->hasArchive());
        });

    /* There is no stackAfter function, so we have to resort to ugly copypasta. */
    auto tooltip = m_timeline.item->timeSlider()->toolTipItem();
    tooltip->stackBefore(m_timeline.item->timeSlider()->bookmarksViewer());
    m_timeline.item->stackBefore(tooltip);
    m_timeline.showButton->stackBefore(m_timeline.item);
    m_timeline.showWidget->stackBefore(m_timeline.showButton);
    m_timeline.resizerWidget->stackBefore(m_timeline.showWidget);

    m_timeline.opacityProcessor = new HoverFocusProcessor(m_controlsWidget);

    enum { kSliderLeaveTimeout = 100 };
    m_timeline.opacityProcessor->setHoverLeaveDelay(kSliderLeaveTimeout);
    m_timeline.item->timeSlider()->bookmarksViewer()->setHoverProcessor(m_timeline.opacityProcessor);
    m_timeline.opacityProcessor->addTargetItem(m_timeline.item);
    m_timeline.opacityProcessor->addTargetItem(m_timeline.item->timeSlider()->toolTipItem());
    m_timeline.opacityProcessor->addTargetItem(m_timeline.showButton);
    m_timeline.opacityProcessor->addTargetItem(m_timeline.resizerWidget);
    m_timeline.opacityProcessor->addTargetItem(m_timeline.showWidget);
    m_timeline.opacityProcessor->addTargetItem(m_timeline.zoomButtonsWidget);

    m_timeline.yAnimator = new VariantAnimator(this);
    m_timeline.yAnimator->setTimer(m_instrumentManager->animationTimer());
    m_timeline.yAnimator->setTargetObject(m_timeline.item);
    m_timeline.yAnimator->setAccessor(new PropertyAccessor("y"));
    m_timeline.yAnimator->setSpeed(70.0 * 2.0);
    m_timeline.yAnimator->setTimeLimit(500);

    m_timeline.opacityAnimatorGroup = new AnimatorGroup(this);
    m_timeline.opacityAnimatorGroup->setTimer(m_instrumentManager->animationTimer());
    m_timeline.opacityAnimatorGroup->addAnimator(opacityAnimator(m_timeline.item));
    m_timeline.opacityAnimatorGroup->addAnimator(opacityAnimator(m_timeline.showButton)); /* Speed of 1.0 is OK here. */

    QnSingleEventEater *sliderWheelEater = new QnSingleEventEater(this);
    sliderWheelEater->setEventType(QEvent::GraphicsSceneWheel);
    m_timeline.resizerWidget->installEventFilter(sliderWheelEater);

    QnSingleEventSignalizer *sliderWheelSignalizer = new QnSingleEventSignalizer(this);
    sliderWheelSignalizer->setEventType(QEvent::GraphicsSceneWheel);
    m_timeline.resizerWidget->installEventFilter(sliderWheelSignalizer);

    connect(sliderZoomInButton, &QnImageButtonWidget::pressed, this,
        [this]
        {
            m_timeline.zoomingIn = true;
        });
    connect(sliderZoomInButton, &QnImageButtonWidget::released, this,
        [this]
        {
            m_timeline.zoomingIn = false;
            m_timeline.item->timeSlider()->hurryKineticAnimations();
        });
    connect(sliderZoomOutButton, &QnImageButtonWidget::pressed, this,
        [this]
        {
            m_timeline.zoomingOut = true;
        });
    connect(sliderZoomOutButton, &QnImageButtonWidget::released, this,
        [this]
        {
            m_timeline.zoomingOut = false;
            m_timeline.item->timeSlider()->hurryKineticAnimations();
        });

    connect(sliderWheelSignalizer, &QnAbstractEventSignalizer::activated, this,
        &QnWorkbenchUi::at_sliderResizerWidget_wheelEvent);
    connect(m_timeline.opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &QnWorkbenchUi::updateSliderOpacityAnimated);
    connect(m_timeline.opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &QnWorkbenchUi::updateSliderOpacityAnimated);
    connect(m_timeline.opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_timeline.opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_timeline.item, &QGraphicsWidget::geometryChanged, this,
        &QnWorkbenchUi::at_sliderItem_geometryChanged);
    connect(m_timeline.resizerWidget, &QGraphicsWidget::geometryChanged, this,
        &QnWorkbenchUi::at_sliderResizerWidget_geometryChanged);

    connect(m_timeline.showingProcessor, &HoverFocusProcessor::hoverEntered, this,
        [this]
        {
            if (!isSliderPinned() && !isSliderOpened())
            {
                setSliderOpened(true);

                /* So that the click that may follow won't hide it. */
                setSliderShowButtonUsed(false);
                QTimer::singleShot(kButtonInactivityTimeoutMs, this, [this]() { setSliderShowButtonUsed(true); });
            }

            m_timeline.hidingProcessor->forceHoverEnter();
        });

    connect(m_timeline.hidingProcessor, &HoverFocusProcessor::hoverFocusLeft, this,
        [this]
        {
            /* Do not auto-hide slider if we have opened context menu. */
            if (!isSliderPinned() && isSliderOpened() && !menu()->isMenuVisible())
                setSliderOpened(false);
        });

    connect(menu(), &QnActionManager::menuAboutToHide, m_timeline.hidingProcessor,
        &HoverFocusProcessor::forceFocusLeave);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);

    if (qnRuntime->isVideoWallMode())
    {
        connect(navigator(), &QnWorkbenchNavigator::positionChanged, this,
            &QnWorkbenchUi::updateCalendarVisibilityAnimated);
        connect(navigator(), &QnWorkbenchNavigator::speedChanged, this,
            &QnWorkbenchUi::updateCalendarVisibilityAnimated);
    }

    connect(action(QnActions::ToggleTourModeAction), &QAction::toggled, this,
        [this](bool toggled)
        {
            /// If tour mode is going to be turned on, focus should be forced to main window
            /// because otherwise we can't cancel tour mode by clicking any key (in some cases)
            if (toggled)
                mainWindow()->setFocus();
        });

    connect(action(QnActions::ToggleTourModeAction), &QAction::toggled, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);

    connect(action(QnActions::ToggleThumbnailsAction), &QAction::toggled, this,
        [this](bool checked)
        {
            setThumbnailsVisible(checked);
        });

    connect(action(QnActions::ToggleSliderAction), &QAction::toggled, this,
        [this](bool checked)
        {
            m_timeline.pinned = checked;
            setSliderOpened(checked);
        });

    const auto getActionParamsFunc =
        [this](const QnCameraBookmark &bookmark) -> QnActionParameters
        {
            QnActionParameters bookmarkParams(currentParameters(Qn::SliderScope));
            bookmarkParams.setArgument(Qn::CameraBookmarkRole, bookmark);
            return bookmarkParams;
        };

    /// TODO: #ynikitenkov move bookmarks-related stuff to new file (BookmarksActionHandler)
    const auto bookmarksViewer = m_timeline.item->timeSlider()->bookmarksViewer();

    const auto updateBookmarkActionsAvailability =
        [this, bookmarksViewer]()
        {
            const bool readonly = qnCommon->isReadOnly()
                || !accessController()->hasGlobalPermission(Qn::GlobalManageBookmarksPermission);

            bookmarksViewer->setReadOnly(readonly);
        };

    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged, this,
        updateBookmarkActionsAvailability);
    connect(qnCommon, &QnCommonModule::readOnlyChanged, this, updateBookmarkActionsAvailability);
    connect(context(), &QnWorkbenchContext::userChanged, this, updateBookmarkActionsAvailability);

    connect(bookmarksViewer, &QnBookmarksViewer::editBookmarkClicked, this,
        [this, getActionParamsFunc](const QnCameraBookmark &bookmark)
        {
            context()->statisticsModule()->registerClick(lit("bookmark_tooltip_edit"));
            menu()->triggerIfPossible(QnActions::EditCameraBookmarkAction, getActionParamsFunc(bookmark));
        });

    connect(bookmarksViewer, &QnBookmarksViewer::removeBookmarkClicked, this,
        [this, getActionParamsFunc](const QnCameraBookmark &bookmark)
        {
            context()->statisticsModule()->registerClick(lit("bookmark_tooltip_delete"));
            menu()->triggerIfPossible(QnActions::RemoveCameraBookmarkAction, getActionParamsFunc(bookmark));
        });

    connect(bookmarksViewer, &QnBookmarksViewer::playBookmark, this,
        [this, getActionParamsFunc](const QnCameraBookmark &bookmark)
        {
            context()->statisticsModule()->registerClick(lit("bookmark_tooltip_play"));

            enum { kMicrosecondsFactor = 1000 };
            navigator()->setPosition(bookmark.startTimeMs * kMicrosecondsFactor);
            navigator()->setPlaying(true);
        });

    connect(bookmarksViewer, &QnBookmarksViewer::tagClicked, this,
        [this, bookmarksViewer](const QString &tag)
        {
            context()->statisticsModule()->registerClick(lit("bookmark_tooltip_tag"));

            QnActionParameters params;
            params.setArgument(Qn::BookmarkTagRole, tag);
            menu()->triggerIfPossible(QnActions::OpenBookmarksSearchAction, params);
        });

    toggleSliderAction->setChecked(settings.state == Qn::PaneState::Opened);

    /* Create a shadow: */
    new QnEdgeShadowWidget(m_timeline.item, Qt::TopEdge, NxUi::kShadowThickness);
}

#pragma endregion Slider methods

#pragma region DebugWidget

#ifdef QN_DEBUG_WIDGET
void QnWorkbenchUi::createDebugWidget()
{
    m_debugOverlayLabel = new QnDebugProxyLabel(m_controlsWidget);
    m_debugOverlayLabel->setAcceptedMouseButtons(0);
    m_debugOverlayLabel->setAcceptHoverEvents(false);
    m_debugOverlayLabel->setMessagesLimit(40);
    setPaletteColor(m_debugOverlayLabel, QPalette::Window, QColor(127, 127, 127, 60));
    setPaletteColor(m_debugOverlayLabel, QPalette::WindowText, QColor(63, 255, 216));
    auto updateDebugGeometry = [&]()
    {
        QPointF pos = QPointF(m_titleItem->geometry().bottomLeft());
        if (qFuzzyEquals(pos, m_debugOverlayLabel->pos()))
            return;
        m_debugOverlayLabel->setPos(pos);
    };
    m_debugOverlayLabel->setVisible(false);
    connect(m_titleItem, &QGraphicsWidget::geometryChanged, this, updateDebugGeometry);
    debugLabel = m_debugOverlayLabel;
    previousMsgHandler = qInstallMessageHandler(uiMsgHandler);

    display()->view()->addAction(action(QnActions::ShowDebugOverlayAction));
    connect(action(QnActions::ShowDebugOverlayAction), &QAction::toggled, this, [&](bool toggled) { m_debugOverlayLabel->setVisible(toggled); });
}
#endif

#pragma endregion Debug overlay methods

#pragma region FpsWidget

bool QnWorkbenchUi::isFpsVisible() const
{
    return m_fpsItem->isVisible();
}

void QnWorkbenchUi::setFpsVisible(bool fpsVisible)
{
    if (fpsVisible == isFpsVisible())
        return;

    m_fpsItem->setVisible(fpsVisible);
    m_fpsCountingInstrument->setEnabled(fpsVisible);

    if (fpsVisible)
        m_fpsCountingInstrument->recursiveEnable();
    else
        m_fpsCountingInstrument->recursiveDisable();

    m_fpsItem->setText(QString());

    action(QnActions::ShowFpsAction)->setChecked(fpsVisible);
}

void QnWorkbenchUi::updateFpsGeometry()
{
    qreal right = m_notificationsBackgroundItem
        ? m_notificationsBackgroundItem->geometry().left()
        : m_controlsWidgetRect.right();

    QPointF pos = QPointF(
        right - m_fpsItem->size().width(),
        m_titleItem ? m_titleItem->geometry().bottom() : 0.0);

    if (qFuzzyEquals(pos, m_fpsItem->pos()))
        return;

    m_fpsItem->setPos(pos);
}

void QnWorkbenchUi::createFpsWidget()
{
    m_fpsItem = new QnProxyLabel(m_controlsWidget);
    m_fpsItem->setAcceptedMouseButtons(0);
    m_fpsItem->setAcceptHoverEvents(false);
    setPaletteColor(m_fpsItem, QPalette::Window, Qt::transparent);
    setPaletteColor(m_fpsItem, QPalette::WindowText, QColor(63, 159, 216));

    display()->view()->addAction(action(QnActions::ShowFpsAction));
    connect(action(QnActions::ShowFpsAction), &QAction::toggled, this, &QnWorkbenchUi::setFpsVisible);
    connect(m_fpsItem, &QGraphicsWidget::geometryChanged, this, &QnWorkbenchUi::updateFpsGeometry);
    setFpsVisible(false);
}

#pragma endregion Fps widget methods
