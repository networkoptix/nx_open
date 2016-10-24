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
#include <ui/graphics/items/generic/blinking_image_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/framed_widget.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/generic/ui_elements_widget.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/controls/navigation_item.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/controls/speed_slider.h>
#include <ui/graphics/items/controls/volume_slider.h>
#include <ui/graphics/items/controls/time_scroll_bar.h>
#include <ui/graphics/items/controls/control_background_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/standard/graphics_web_view.h>
#include <ui/graphics/items/controls/bookmarks_viewer.h>
#include <ui/graphics/items/notifications/notifications_collection_widget.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/processors/hover_processor.h>

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
#include <ui/workbench/panels/buttons.h>

#include "workbench.h"
#include "workbench_display.h"
#include "workbench_layout.h"
#include "workbench_context.h"
#include "workbench_navigator.h"
#include "workbench_access_controller.h"
#include "workbench_pane_settings.h"
#include <ui/workbench/panels/resources_workbench_panel.h>
#include <ui/workbench/panels/notifications_workbench_panel.h>
#include <ui/workbench/panels/timeline_workbench_panel.h>
#include <ui/workbench/panels/calendar_workbench_panel.h>

#include <nx/fusion/model_functions.h>

#ifdef _DEBUG
//#define QN_DEBUG_WIDGET
#endif


namespace {

Qn::PaneState makePaneState(bool opened, bool pinned = true)
{
    return pinned ? (opened ? Qn::PaneState::Opened : Qn::PaneState::Closed) : Qn::PaneState::Unpinned;
}

const int kHideControlsTimeoutMs = 2000;


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
    m_flags(0),
    m_titleUsed(false),
    m_titleVisible(false),
    m_ignoreClickEvent(false),
    m_inactive(false),
    m_inFreespace(false)
{
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
    if (qnRuntime->isDesktopMode())
        createTreeWidget(settings[Qn::WorkbenchPane::Tree]);

    /* Title bar. */
    if (qnRuntime->isDesktopMode())
        createTitleWidget(settings[Qn::WorkbenchPane::Title]);

    /* Notifications. */
    if (qnRuntime->isDesktopMode()
        && !qnSettings->lightMode().testFlag(Qn::LightModeNoNotifications))
    {
        createNotificationsWidget(settings[Qn::WorkbenchPane::Notifications]);
    }

    /* Calendar. */
    createCalendarWidget(settings[Qn::WorkbenchPane::Calendar]);

    /* Navigation slider. */
    createTimelineWidget(settings[Qn::WorkbenchPane::Navigation]);

    /* Make timeline panel aware of calendar panel. */
    m_timeline->setCalendarPanel(m_calendar);

    /* Windowed title shadow. */
    auto windowedTitleShadow = new QnEdgeShadowWidget(m_controlsWidget,
        Qt::TopEdge, NxUi::kShadowThickness, true);
    windowedTitleShadow->setZValue(NxUi::ShadowItemZOrder);

#ifdef QN_DEBUG_WIDGET
    /* Debug overlay */
    createDebugWidget();
#endif

    initGraphicsMessageBoxHolder();

    /* Connect to display. */
    display()->view()->addAction(action(QnActions::FreespaceAction));
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

    setTimelineVisible(false, false);
    setTreeVisible(true, false);
    setTitleVisible(true, false);
    setTitleUsed(false);
    setNotificationsVisible(true, false);
    updateControlsVisibility(false);

    setTreeOpened(settings[Qn::WorkbenchPane::Tree].state == Qn::PaneState::Opened, false);
    setTitleOpened(settings[Qn::WorkbenchPane::Title].state == Qn::PaneState::Opened, false);
    setTimelineOpened(settings[Qn::WorkbenchPane::Navigation].state != Qn::PaneState::Closed, false);
    setNotificationsOpened(settings[Qn::WorkbenchPane::Notifications].state == Qn::PaneState::Opened, false);
    setCalendarOpened(settings[Qn::WorkbenchPane::Calendar].state == Qn::PaneState::Opened, false);

    m_timeline->lastThumbnailsHeight = settings[Qn::WorkbenchPane::Thumbnails].span;
    m_timeline->setThumbnailsVisible(settings[Qn::WorkbenchPane::Thumbnails].state == Qn::PaneState::Opened);

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

    if (m_tree)
    {
        QnPaneSettings& tree = settings[Qn::WorkbenchPane::Tree];
        tree.state = makePaneState(isTreeOpened(), isTreePinned());
        tree.span = m_tree->item->geometry().width();
    }

    QnPaneSettings& notifications = settings[Qn::WorkbenchPane::Notifications];
    notifications.state = makePaneState(isNotificationsOpened(), isNotificationsPinned());

    QnPaneSettings& navigation = settings[Qn::WorkbenchPane::Navigation];
    navigation.state = makePaneState(isTimelineOpened(), true);

    QnPaneSettings& calendar = settings[Qn::WorkbenchPane::Calendar];
    calendar.state = makePaneState(isCalendarOpened(), isCalendarPinned());

    QnPaneSettings& thumbnails = settings[Qn::WorkbenchPane::Thumbnails];
    thumbnails.state = makePaneState(m_timeline->isThumbnailsVisible());
    thumbnails.span = m_timeline->lastThumbnailsHeight;

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
    if (m_tree && focusItem == m_tree->item)
        return Qn::TreeScope;

    if (focusItem == m_timeline->item)
        return Qn::TimelineScope;

    if (!focusItem || dynamic_cast<QnResourceWidget*>(focusItem))
        return Qn::SceneScope;

    /* We should not handle any button as an action while the item was focused. */
    if (dynamic_cast<QnGraphicsWebView*>(focusItem))
        return Qn::InvalidScope;

    return Qn::MainScope;
}

QnActionParameters QnWorkbenchUi::currentParameters(Qn::ActionScope scope) const
{
    /* Get items. */
    switch (scope)
    {
        case Qn::TreeScope:
            return m_tree ? m_tree->widget->currentParameters(scope) : QnActionParameters();
        case Qn::TimelineScope:
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

void QnWorkbenchUi::updateControlsVisibility(bool animate)
{    // TODO
    ensureAnimationAllowed(animate);

    const auto calculateTimelineVisible = [this]()
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

    bool timelineVisible = calculateTimelineVisible();

    if (qnRuntime->isVideoWallMode())
    {
        setTimelineVisible(timelineVisible, animate);
        setTreeVisible(false, false);
        setTitleVisible(false, false);
        setNotificationsVisible(false, false);
        return;
    }

    if (m_inactive)
    {
        bool hovered = isHovered();
        setTimelineVisible(timelineVisible && hovered, animate);
        setTreeVisible(hovered, animate);
        setTitleVisible(hovered, animate);
        setNotificationsVisible(hovered, animate);
    }
    else
    {
        setTimelineVisible(timelineVisible, false);
        setTreeVisible(true, animate);
        setTitleVisible(true, animate);
        setNotificationsVisible(true, animate);
    }

    updateCalendarVisibility(animate);
}

void QnWorkbenchUi::updateTitleOpacityAnimated()
{
    updateTitleOpacity(true);
}

void QnWorkbenchUi::updateCalendarVisibilityAnimated()
{
    updateCalendarVisibility(true);
}

void QnWorkbenchUi::updateControlsVisibilityAnimated()
{
    updateControlsVisibility(true);
}

QMargins QnWorkbenchUi::calculateViewportMargins(qreal treeX, qreal treeW, qreal titleY, qreal titleH, qreal sliderY, qreal notificationsX)
{
    QMargins result(
        isTreePinned() ? std::floor(qMax(0.0, treeX + treeW)) : 0.0,
        std::floor(qMax(0.0, titleY + titleH)),
        m_notifications ? std::floor(qMax(0.0, isNotificationsPinned() ? m_controlsWidgetRect.right() - notificationsX : 0.0)) : 0.0,
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
            m_tree ? (m_tree->xAnimator->isRunning() ? m_tree->xAnimator->targetValue().toReal() : m_tree->item->pos().x()) : 0.0,
            m_tree ? m_tree->item->size().width() : 0.0,
            m_titleYAnimator ? (m_titleYAnimator->isRunning() ? m_titleYAnimator->targetValue().toReal() : m_titleItem->pos().y()) : 0.0,
            m_titleItem ? m_titleItem->size().height() : 0.0,
            m_timeline->effectiveGeometry().top(),
            m_notifications ? m_notifications->xAnimator->isRunning() ? m_notifications->xAnimator->targetValue().toReal() : m_notifications->item->pos().x() : 0.0
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
        (m_timeline->isHovered())
        || (m_tree && m_tree->isHovered())
        || (m_titleOpacityProcessor         && m_titleOpacityProcessor->isHovered())
        || (m_notifications && m_notifications->isHovered())
        || (m_calendar && m_calendar->isHovered());
}

QnWorkbenchUi::Panels QnWorkbenchUi::openedPanels() const
{
    return
        (isTreeOpened() ? TreePanel : NoPanel) |
        (isTitleOpened() ? TitlePanel : NoPanel) |
        (isTimelineOpened() ? TimelinePanel : NoPanel) |
        (isNotificationsOpened() ? NotificationsPanel : NoPanel);
}

void QnWorkbenchUi::setOpenedPanels(Panels panels, bool animate)
{
    ensureAnimationAllowed(animate);

    setTreeOpened(panels & TreePanel, animate);
    setTitleOpened(panels & TitlePanel, animate);
    setTimelineOpened(panels & TimelinePanel, animate);
    setNotificationsOpened(panels & NotificationsPanel, animate);
}

void QnWorkbenchUi::initGraphicsMessageBoxHolder()
{
    auto overlayWidget = new QnUiElementsWidget();
    overlayWidget->setAcceptedMouseButtons(0);
    display()->scene()->addItem(overlayWidget);
    display()->setLayer(overlayWidget, Qn::MessageBoxLayer);

    QGraphicsLinearLayout* messageBoxVLayout = new QGraphicsLinearLayout(Qt::Vertical);
    messageBoxVLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    messageBoxVLayout->setSpacing(0.0);

    QGraphicsLinearLayout* messageBoxHLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    messageBoxHLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    messageBoxHLayout->setSpacing(0.0);

    overlayWidget->setLayout(messageBoxHLayout);

    messageBoxHLayout->addStretch();
    messageBoxHLayout->addItem(messageBoxVLayout);
    messageBoxHLayout->addStretch();

    messageBoxVLayout->addStretch();
    messageBoxVLayout->addItem(new QnGraphicsMessageBoxHolder(overlayWidget));
    messageBoxVLayout->addStretch();
}

void QnWorkbenchUi::tick(int deltaMSecs)
{
    if (!m_timeline->zoomingIn && !m_timeline->zoomingOut)
        return;

    if (!display()->scene())
        return;

    auto slider = m_timeline->item->timeSlider();

    QPointF pos;
    if (slider->windowStart() <= slider->sliderPosition() && slider->sliderPosition() <= slider->windowEnd())
        pos = slider->positionFromValue(slider->sliderPosition(), true);
    else
        pos = slider->rect().center();

    QGraphicsSceneWheelEvent event(QEvent::GraphicsSceneWheel);
    event.setDelta(360 * 8 * (m_timeline->zoomingIn ? deltaMSecs : -deltaMSecs) / 1000); /* 360 degrees per sec, x8 since delta is measured in in eighths (1/8s) of a degree. */
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
    {
        m_inFreespace = isFullscreen
           && (isTreePinned() && !isTreeOpened())
           && !isTitleOpened()
           && (isNotificationsPinned() && !isNotificationsOpened())
           && (isTimelinePinned() && !isTimelineOpened());
    }

    if (!m_inFreespace)
    {
        storeSettings();

        if (!isFullscreen)
            fullScreenAction->setChecked(true);

        setTreeOpened(false, isFullscreen);
        setTitleOpened(false, isFullscreen);
        setTimelineOpened(false, isFullscreen);
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
        setTimelineOpened(settings[Qn::WorkbenchPane::Navigation].state == Qn::PaneState::Opened, isFullscreen);
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
            setOpenedPanels(m_unzoomedOpenedPanels | openedPanels(), true);

            /* Viewport margins have changed, force fit-in-view. */
            display()->fitInView();
        }
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
                    setTimelineVisible(false, true);
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

    m_controlsWidgetRect = rect;

    /* We lay everything out manually. */

    m_timeline->updateGeometry();


    if (m_titleItem)
    {
        m_titleItem->setGeometry(QRectF(
            0.0,
            m_titleItem->pos().y(),
            rect.width(),
            m_titleItem->size().height()));
    }

    if (m_notifications)
    {
        if (m_notifications->xAnimator->isRunning())
            m_notifications->xAnimator->stop();
        m_notifications->item->setX(rect.right() + (isNotificationsOpened() ? -m_notifications->item->size().width() : 1.0 /* Just in case. */));
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

void QnWorkbenchUi::setTreeVisible(bool visible, bool animate)
{
    if (m_tree)
        m_tree->setVisible(visible, animate);
}

bool QnWorkbenchUi::isTreePinned() const
{
    return m_tree && m_tree->isPinned();
}

bool QnWorkbenchUi::isTreeVisible() const
{
    return m_tree && m_tree->isVisible();
}

bool QnWorkbenchUi::isTreeOpened() const
{
    return m_tree && m_tree->isOpened();
}

void QnWorkbenchUi::setTreeOpened(bool opened, bool animate)
{
    if (!m_tree)
        return;
    m_tree->setOpened(opened, animate);
}

QRectF QnWorkbenchUi::updatedTreeGeometry(const QRectF &treeGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry)
{
    QPointF pos(
        treeGeometry.x(),
        ((!m_titleVisible || !m_titleUsed) && isTreeVisible()) ? 0.0 : qMax(titleGeometry.bottom(), 0.0));

    QSizeF size(
        treeGeometry.width(),
        ((!m_timeline->isVisible() && isTreeVisible())
            ? m_controlsWidgetRect.bottom()
            : qMin(sliderGeometry.y(), m_controlsWidgetRect.bottom())) - pos.y());

    return QRectF(pos, size);
}

void QnWorkbenchUi::updateTreeGeometry()
{
    if (!m_tree)
        return;

    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedTreeGeometry(m_tree->item->geometry(), m_titleItem->geometry(), m_timeline->item->geometry());
    m_tree->item->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_tree->item->setPos(geometry.topLeft());

    /* Whether actual size change should be deferred. */
    bool defer = false;

    /* Calculate slider target position. */
    QPointF sliderPos;
    if (!m_timeline->isVisible() && isTreeVisible())
    {
        sliderPos = QPointF(m_timeline->item->pos().x(), m_controlsWidgetRect.bottom());
    }
    else
    {
        sliderPos = m_timeline->effectiveGeometry().topLeft();
        defer |= !qFuzzyEquals(sliderPos, m_timeline->item->pos()); /* If animation is running, then geometry sync should be deferred. */
    }

    /* Calculate title target position. */
    QPointF titlePos;
    if ((!m_titleVisible || !m_titleUsed) && isTreeVisible())
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
    geometry = updatedTreeGeometry(m_tree->item->geometry(), QRectF(titlePos, m_titleItem->size()), QRectF(sliderPos, m_timeline->item->size()));
    if (qFuzzyEquals(geometry, m_tree->item->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if (defer && geometry.height() < m_tree->item->size().height())
        return;

    m_tree->item->resize(geometry.size());
}

void QnWorkbenchUi::createTreeWidget(const QnPaneSettings& settings)
{
    m_tree = new NxUi::ResourceTreeWorkbenchPanel(settings, m_controlsWidget, this);

    connect(m_tree, &NxUi::AbstractWorkbenchPanel::openedChanged, this,
        [this](bool opened)
        {
            if (opened && m_tree->isPinned())
                m_inFreespace = false;
        });
    connect(m_tree, &NxUi::AbstractWorkbenchPanel::visibleChanged, this,
        &QnWorkbenchUi::updateTreeGeometry);

    connect(m_tree, &NxUi::AbstractWorkbenchPanel::hoverEntered, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_tree, &NxUi::AbstractWorkbenchPanel::hoverLeft, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);

    connect(m_tree, &NxUi::AbstractWorkbenchPanel::geometryChanged, this,
        &QnWorkbenchUi::updateViewportMargins);

}

#pragma endregion Tree widget methods

#pragma region TitleWidget

bool QnWorkbenchUi::isTitleVisible() const
{
    return m_titleVisible;
}

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

void QnWorkbenchUi::setTitleOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(animate);

    if (!m_titleItem)
        return;

    if (animate)
    {
        m_titleOpacityAnimatorGroup->pause();
        opacityAnimator(m_titleItem)->setTargetValue(opacity);
        opacityAnimator(m_titleShowButton)->setTargetValue(opacity);
        m_titleOpacityAnimatorGroup->start();
    }
    else
    {
        m_titleOpacityAnimatorGroup->stop();
        m_titleItem->setOpacity(opacity);
        m_titleShowButton->setOpacity(opacity);
    }
}

void QnWorkbenchUi::updateTitleOpacity(bool animate)
{
    const qreal opacity = m_titleVisible ? NxUi::kOpaque : NxUi::kHidden;
    setTitleOpacity(opacity, animate);
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
    m_titleYAnimator->setSpeed(1.0);
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
    auto shadow = new QnEdgeShadowWidget(m_titleItem, Qt::BottomEdge, NxUi::kShadowThickness);
    shadow->setZValue(NxUi::ShadowItemZOrder);
}

#pragma endregion Title methods

#pragma region NotificationsWidget

bool QnWorkbenchUi::isNotificationsVisible() const
{
    return m_notifications && m_notifications->isVisible();
}

bool QnWorkbenchUi::isNotificationsOpened() const
{
    return m_notifications && m_notifications->isOpened();
}

bool QnWorkbenchUi::isNotificationsPinned() const
{
    return m_notifications && m_notifications->isPinned();
}

void QnWorkbenchUi::setNotificationsOpened(bool opened, bool animate)
{
    if (m_notifications)
        m_notifications->setOpened(opened, animate);
}

void QnWorkbenchUi::setNotificationsVisible(bool visible, bool animate)
{
    if (m_notifications)
        m_notifications->setVisible(visible, animate);
}

QRectF QnWorkbenchUi::updatedNotificationsGeometry(const QRectF &notificationsGeometry, const QRectF &titleGeometry)
{
    QPointF pos(
        notificationsGeometry.x(),
        ((!m_titleVisible || !m_titleUsed) && isNotificationsVisible()) ? 0.0 : qMax(titleGeometry.bottom(), 0.0));

    qreal top = m_controlsWidgetRect.bottom();
    if (m_calendar && m_calendar->isOpened() && m_calendar->isEnabled())
        top = m_calendar->effectiveGeometry().top();
    else if (m_timeline->isVisible() && m_timeline->isOpened())
        top = m_timeline->effectiveGeometry().top();

    const qreal maxHeight = top - pos.y();

    QSizeF size(notificationsGeometry.width(), maxHeight);
    return QRectF(pos, size);
}

void QnWorkbenchUi::updateNotificationsGeometry()
{
    if (!m_notifications)
        return;

    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedNotificationsGeometry(m_notifications->item->geometry(),
        m_titleItem->geometry());

    /* Always change position. */
    m_notifications->item->setPos(geometry.topLeft());

    /* Whether actual size change should be deferred. */
    bool defer = false;

    /* Calculate slider target position. */
    QPointF sliderPos;
    if (!m_timeline->isVisible() && isNotificationsVisible())
    {
        sliderPos = QPointF(m_timeline->item->pos().x(), m_controlsWidgetRect.bottom());
    }
    else
    {
        sliderPos = m_timeline->effectiveGeometry().topLeft();
        defer |= !qFuzzyEquals(sliderPos, m_timeline->item->pos()); /* If animation is running, then geometry sync should be deferred. */
    }

    /* Calculate title target position. */
    QPointF titlePos;
    if ((!m_titleVisible || !m_titleUsed) && isNotificationsVisible())
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
    geometry = updatedNotificationsGeometry(m_notifications->item->geometry(),
        QRectF(titlePos, m_titleItem->size()));

    if (qFuzzyEquals(geometry, m_notifications->item->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if (defer && geometry.height() < m_notifications->item->size().height())
        return;

    m_notifications->item->resize(geometry.size());

    /* All tooltips should fit to rect of maxHeight */
    QRectF tooltipsEnclosingRect(
        m_controlsWidgetRect.left(),
        m_notifications->item->y(),
        m_controlsWidgetRect.width(),
        m_notifications->item->geometry().height());
    m_notifications->item->setToolTipsEnclosingRect(m_controlsWidget->mapRectToItem(m_notifications->item, tooltipsEnclosingRect));
}

void QnWorkbenchUi::createNotificationsWidget(const QnPaneSettings& settings)
{
    m_notifications = new NxUi::NotificationsWorkbenchPanel(settings, m_controlsWidget, this);
    connect(m_notifications, &NxUi::AbstractWorkbenchPanel::openedChanged, this,
        [this](bool opened)
        {
            if (opened && m_notifications->isPinned())
                m_inFreespace = false;
        });
    connect(m_notifications, &NxUi::AbstractWorkbenchPanel::visibleChanged, this,
        &QnWorkbenchUi::updateNotificationsGeometry);

    connect(m_notifications, &NxUi::AbstractWorkbenchPanel::hoverEntered, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_notifications, &NxUi::AbstractWorkbenchPanel::hoverLeft, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);

    connect(m_notifications, &NxUi::AbstractWorkbenchPanel::geometryChanged, this,
        &QnWorkbenchUi::updateViewportMargins);
    connect(m_notifications, &NxUi::AbstractWorkbenchPanel::geometryChanged, this,
        &QnWorkbenchUi::updateFpsGeometry);
}

#pragma endregion Notifications widget methods

#pragma region CalendarWidget

bool QnWorkbenchUi::isCalendarPinned() const
{
    return m_calendar && m_calendar->isPinned();
}

bool QnWorkbenchUi::isCalendarOpened() const
{
    return m_calendar && m_calendar->isOpened();
}

bool QnWorkbenchUi::isCalendarVisible() const
{
    return m_calendar && m_calendar->isVisible();
}

void QnWorkbenchUi::setCalendarOpened(bool opened, bool animate)
{
    if (m_calendar)
        m_calendar->setOpened(opened, animate);
}

void QnWorkbenchUi::updateCalendarVisibility(bool animate)
{
    if (!m_calendar)
        return;

    /* Small hack. We have a signal that updates visibility if a calendar receive new data */
    bool calendarEmpty = navigator()->calendar()->isEmpty();
    //TODO: #GDM refactor to the same logic as timeline

    bool calendarEnabled = !calendarEmpty
        && navigator()->currentWidget()
        && navigator()->currentWidget()->resource()->flags().testFlag(Qn::utc)
        && (!m_inactive || m_timeline->isHovered())
        && m_timeline->isVisible()
        && m_timeline->isOpened();

    /* Avoid double notifications geometry calculations. */
    if (m_calendar->isEnabled() == calendarEnabled)
        return;

    m_calendar->setEnabled(calendarEnabled, animate);
    updateNotificationsGeometry();
}

void QnWorkbenchUi::updateCalendarGeometry()
{
    QRectF geometry = m_calendar->item->geometry();
    geometry.moveRight(m_controlsWidgetRect.right());
    geometry.moveBottom(m_timeline->effectiveGeometry().top());
    m_calendar->setOrigin(geometry.topLeft());
}

void QnWorkbenchUi::createCalendarWidget(const QnPaneSettings& settings)
{
    m_calendar = new NxUi::CalendarWorkbenchPanel(settings, m_controlsWidget, this);

    connect(m_calendar, &NxUi::AbstractWorkbenchPanel::visibleChanged, this,
        &QnWorkbenchUi::updateNotificationsGeometry);

    connect(m_calendar, &NxUi::AbstractWorkbenchPanel::geometryChanged, this,
        &QnWorkbenchUi::updateNotificationsGeometry);

    //TODO: #GDM refactor indirect dependency
    connect(navigator()->calendar(), &QnCalendarWidget::emptyChanged, this,
        &QnWorkbenchUi::updateCalendarVisibilityAnimated);

    connect(m_calendar, &NxUi::AbstractWorkbenchPanel::hoverEntered, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_calendar, &NxUi::AbstractWorkbenchPanel::hoverLeft, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
}

#pragma endregion Calendar and DayTime widget methods

#pragma region TimelineWidget

bool QnWorkbenchUi::isTimelineVisible() const
{
    return m_timeline->isVisible();
}

bool QnWorkbenchUi::isTimelineOpened() const
{
    return m_timeline->isOpened();
}

bool QnWorkbenchUi::isTimelinePinned() const
{
    /* Auto-hide slider when some item is zoomed, otherwise - don't. */
    return m_timeline->isPinned() ||
        m_widgetByRole[Qn::ZoomedRole] == nullptr;
}

void QnWorkbenchUi::setTimelineOpened(bool opened, bool animate)
{
    if (qnRuntime->isVideoWallMode())
        opened = true;

    m_timeline->setOpened(opened, animate);
}

void QnWorkbenchUi::setTimelineVisible(bool visible, bool animate)
{
    m_timeline->setVisible(visible, animate);
}

void QnWorkbenchUi::createTimelineWidget(const QnPaneSettings& settings)
{
    m_timeline = new NxUi::TimelineWorkbenchPanel(settings, m_controlsWidget, this);

    connect(m_timeline, &NxUi::AbstractWorkbenchPanel::openedChanged, this,
        [this](bool opened, bool animated)
        {
            if (opened && m_timeline->isPinned())
                m_inFreespace = false;
            updateCalendarVisibility(animated);
        });

    connect(m_timeline, &NxUi::AbstractWorkbenchPanel::visibleChanged, this,
        [this](bool /*value*/, bool animated)
        {
            updateTreeGeometry();
            updateCalendarVisibility(animated);
        });

    connect(m_timeline, &NxUi::AbstractWorkbenchPanel::hoverEntered, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_timeline, &NxUi::AbstractWorkbenchPanel::hoverLeft, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);

    connect(m_timeline, &NxUi::AbstractWorkbenchPanel::geometryChanged, this,
        [this]
        {
            updateTreeGeometry();
            updateNotificationsGeometry();
            updateViewportMargins();
            updateCalendarGeometry();
        });

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged, this,
        &QnWorkbenchUi::updateControlsVisibilityAnimated);

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

    const auto getActionParamsFunc =
        [this](const QnCameraBookmark &bookmark) -> QnActionParameters
        {
            QnActionParameters bookmarkParams(currentParameters(Qn::TimelineScope));
            bookmarkParams.setArgument(Qn::CameraBookmarkRole, bookmark);
            return bookmarkParams;
        };

    /// TODO: #ynikitenkov move bookmarks-related stuff to new file (BookmarksActionHandler)
    const auto bookmarksViewer = m_timeline->item->timeSlider()->bookmarksViewer();

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
}

#pragma endregion Timeline methods

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
    qreal right = m_notifications
        ? m_notifications->backgroundItem->geometry().left()
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
