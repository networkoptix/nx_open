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
    m_fpsCountingInstrument(nullptr),
    m_controlsActivityInstrument(nullptr),
    m_flags(0),
    m_controlsWidget(nullptr),
    m_titleUsed(false),
    m_titleVisible(false),
    m_calendarVisible(false),
    m_dayTimeOpened(false),
    m_ignoreClickEvent(false),
    m_inactive(false),
    m_fpsItem(nullptr),
    m_debugOverlayLabel(nullptr),

    m_inFreespace(false),
    m_unzoomedOpenedPanels(),

    m_timeline(),
    m_tree(nullptr),

    m_titleItem(nullptr),
    m_titleShowButton(nullptr),
    m_titleOpacityAnimatorGroup(nullptr),
    m_titleYAnimator(nullptr),
    m_titleOpacityProcessor(nullptr),

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
    if (!qnSettings->lightMode().testFlag(Qn::LightModeNoNotifications))
        createNotificationsWidget(settings[Qn::WorkbenchPane::Notifications]);

    /* Calendar. */
    createCalendarWidget(settings[Qn::WorkbenchPane::Calendar]);

    /* Navigation slider. */
    createSliderWidget(settings[Qn::WorkbenchPane::Navigation]);

    /* Windowed title shadow. */
    auto windowedTitleShadow = new QnEdgeShadowWidget(m_controlsWidget,
        Qt::TopEdge, NxUi::kShadowThickness, true);
    windowedTitleShadow->setZValue(NxUi::ShadowItemZOrder);

#ifdef QN_DEBUG_WIDGET
    /* Debug overlay */
    createDebugWidget();
#endif

    initGraphicsMessageBox();

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
    navigation.state = makePaneState(isSliderOpened(), isSliderPinned());

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
            return m_tree ? m_tree->widget->currentParameters(scope) : QnActionParameters();
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
            m_timeline->yAnimator->isRunning() ? m_timeline->yAnimator->targetValue().toReal() : m_timeline->item->pos().y(),
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
    if (!m_timeline->zoomingIn && !m_timeline->zoomingOut)
        return;

    if (!display()->scene())
        return;

    QnTimeSlider *slider = m_timeline->item->timeSlider();

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

        m_timeline->showWidget->setVisible(newWidget);
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
    else if (m_timeline->yAnimator->isRunning())
    {
        sliderPos = QPointF(m_timeline->item->pos().x(), m_timeline->yAnimator->targetValue().toReal());
        defer |= !qFuzzyEquals(sliderPos, m_timeline->item->pos()); /* If animation is running, then geometry sync should be deferred. */
    }
    else
    {
        sliderPos = m_timeline->item->pos();
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
            if (opened)
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

QRectF QnWorkbenchUi::updatedNotificationsGeometry(const QRectF &notificationsGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry)
{
    QPointF pos(
        notificationsGeometry.x(),
        ((!m_titleVisible || !m_titleUsed) && isNotificationsVisible()) ? 0.0 : qMax(titleGeometry.bottom(), 0.0));

    const qreal maxHeight = (m_timeline->isVisible() ? sliderGeometry.y() : m_controlsWidgetRect.bottom()) - pos.y();

    QSizeF size(notificationsGeometry.width(), maxHeight);
    return QRectF(pos, size);
}

void QnWorkbenchUi::updateNotificationsGeometry()
{
    if (!m_notifications)
        return;

    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedNotificationsGeometry(m_notifications->item->geometry(), m_titleItem->geometry(), m_timeline->item->geometry());

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
    else if (m_timeline->yAnimator->isRunning())
    {
        sliderPos = QPointF(m_timeline->item->pos().x(), m_timeline->yAnimator->targetValue().toReal());
        defer |= !qFuzzyEquals(sliderPos, m_timeline->item->pos()); /* If animation is running, then geometry sync should be deferred. */
    }
    else
    {
        sliderPos = m_timeline->item->pos();
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
        QRectF(titlePos, m_titleItem->size()),
        QRectF(sliderPos, m_timeline->item->size()));

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
            if (opened)
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
    return action(QnActions::PinCalendarAction)->isChecked();
}

bool QnWorkbenchUi::isCalendarOpened() const
{
    return action(QnActions::ToggleCalendarAction)->isChecked();
}

bool QnWorkbenchUi::isCalendarVisible() const
{
    return m_calendarVisible;
}

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
    const qreal opacity = m_calendarVisible ? NxUi::kOpaque : NxUi::kHidden;
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

    bool calendarVisible = calendarEnabled && m_timeline->isVisible() && isSliderOpened();
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
    QRectF geometry = updatedCalendarGeometry(m_timeline->item->geometry());
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
    QRectF geometry = updatedDayTimeWidgetGeometry(m_timeline->item->geometry(), m_calendarItem->geometry());
    m_dayTimeItem->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_dayTimeItem->setPos(geometry.topLeft());
}

void QnWorkbenchUi::setCalendarShowButtonUsed(bool used)
{
    m_timeline->item->calendarButton()->setAcceptedMouseButtons(used ? Qt::LeftButton : Qt::NoButton);
}

void QnWorkbenchUi::at_calendarShowingProcessor_hoverEntered()
{
    if (!isCalendarPinned() && !isCalendarOpened())
    {
        setCalendarOpened(true);

        /* So that the click that may follow won't hide it. */
        setCalendarShowButtonUsed(false);
        QTimer::singleShot(NxUi::kButtonInactivityTimeoutMs, this, [this]() { setCalendarShowButtonUsed(true); });
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
    m_calendarHidingProcessor->setHoverLeaveDelay(NxUi::kClosePanelTimeoutMs);
    m_calendarHidingProcessor->setFocusLeaveDelay(NxUi::kClosePanelTimeoutMs);

    m_calendarShowingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_calendarShowingProcessor->setHoverEnterDelay(NxUi::kOpenPanelTimeoutMs);

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

bool QnWorkbenchUi::isSliderVisible() const
{
    return m_timeline->isVisible();
}

bool QnWorkbenchUi::isSliderOpened() const
{
    return m_timeline->isOpened();
}

bool QnWorkbenchUi::isSliderPinned() const
{
    /* Auto-hide slider when some item is zoomed, otherwise - don't. */
    return m_timeline->isPinned() ||
        m_widgetByRole[Qn::ZoomedRole] == nullptr;
}

void QnWorkbenchUi::setSliderOpened(bool opened, bool animate)
{
    if (qnRuntime->isVideoWallMode())
        opened = true;

    /* Do not hide pinned timeline. */
    if (m_timeline->isPinned() && !opened)
        return;

    m_timeline->setOpened(opened, animate);

    updateCalendarVisibility(animate);
    m_inFreespace &= !opened;
}

void QnWorkbenchUi::setSliderVisible(bool visible, bool animate)
{
    m_timeline->setVisible(visible, animate);
}

void QnWorkbenchUi::createSliderWidget(const QnPaneSettings& settings)
{
    m_timeline = new NxUi::TimelineWorkbenchPanel(settings, m_controlsWidget, this);

    connect(m_timeline, &NxUi::AbstractWorkbenchPanel::visibleChanged, this,
        [this](bool value, bool animated)
        {
            updateTreeGeometry();
            updateNotificationsGeometry();
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
        updateDayTimeWidgetGeometry();
    });

    /*
    Calendar is created before navigation slider (alot of logic relies on that).
    Therefore we have to bind calendar showing/hiding processors to navigation
    pane button "CLND" here and not in createCalendarWidget()
    */
    m_calendarHidingProcessor->addTargetItem(m_timeline->item->calendarButton());

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
            m_timeline->setThumbnailsVisible(checked);
        });

    const auto getActionParamsFunc =
        [this](const QnCameraBookmark &bookmark) -> QnActionParameters
        {
            QnActionParameters bookmarkParams(currentParameters(Qn::SliderScope));
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
