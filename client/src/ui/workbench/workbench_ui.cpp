#include "workbench_ui.h"
#include <cassert>
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

#include <core/dataprovider/abstract_streamdataprovider.h>

#include <core/resource/security_cam_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>

#include <camera/resource_display.h>

#include <ui/animation/viewport_animator.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>

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
#include <ui/graphics/items/notifications/notifications_collection_widget.h>
#include <ui/common/palette.h>
#include <ui/processors/hover_processor.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/calendar_widget.h>
#include <ui/widgets/day_time_widget.h>
#include <ui/widgets/resource_browser_widget.h>
#include <ui/widgets/layout_tab_bar.h>
#include <ui/widgets/search_line_edit.h>
#include <ui/widgets/main_window.h>
#include <ui/style/skin.h>
#include <ui/style/noptix_style.h>
#include <ui/workaround/qtbug_workaround.h>
#include <ui/screen_recording/screen_recorder.h>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/checked_cast.h>
#include <utils/common/counter.h>
#include <client/client_settings.h>

#include "watchers/workbench_render_watcher.h"
#include "workbench.h"
#include "workbench_display.h"
#include "workbench_layout.h"
#include "workbench_context.h"
#include "workbench_navigator.h"
#include "workbench_access_controller.h"


namespace {

    const QSize showHideButtonSize(15, 45);
    const QMargins showHideButtonMargins(0, 10, 0, 10);

    const qreal kDefaultSizeMultiplier = 1.0;
    const int kDefaultHelpTopicId = -1;

    QnImageButtonWidget *newActionButton(QAction *action, qreal sizeMultiplier = kDefaultSizeMultiplier
        , int helpTopicId = kDefaultHelpTopicId, QGraphicsItem *parent = nullptr)
    {
        int baseSize = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, NULL, NULL);

        qreal height = baseSize * sizeMultiplier;
        qreal width = height * QnGeometry::aspectRatio(action->icon().actualSize(QSize(1024, 1024)));

        QnImageButtonWidget *button;

        qreal rotationSpeed = action->property(Qn::ToolButtonCheckedRotationSpeed).toReal();
        if(!qFuzzyIsNull(rotationSpeed)) {
            QnRotatingImageButtonWidget *rotatingButton = new QnRotatingImageButtonWidget(parent);
            rotatingButton->setRotationSpeed(rotationSpeed);
            button = rotatingButton;
        } else {
            button = new QnImageButtonWidget(parent);
        }

        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::ToolButton);
        button->setFixedSize(width, height);
        button->setDefaultAction(action);
        button->setCached(true);

        if(helpTopicId != -1)
            setHelpTopic(button, helpTopicId);

        return button;
    }

    QnImageButtonWidget *newShowHideButton(QGraphicsItem *parent = NULL, QAction *action = NULL) {
        QnImageButtonWidget *button = new QnImageButtonWidget(parent);
        button->setFixedSize(showHideButtonSize);
        button->setImageMargins(showHideButtonMargins);
        if (action)
            button->setDefaultAction(action);
        else
            button->setCheckable(true);
        button->setIcon(qnSkin->icon("panel/slide_right.png", "panel/slide_left.png"));
        
        button->setProperty(Qn::NoHandScrollOver, true);
        button->setCached(true);

        setHelpTopic(button, Qn::MainWindow_Pin_Help);
        return button;
    }

    QnImageButtonWidget *newPinButton(QGraphicsItem *parent = NULL, QAction *action = NULL) {
        QnImageButtonWidget *button = new QnImageButtonWidget(parent);

        int size = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, NULL, NULL);
        button->resize(size, size);
        if (action)
            button->setDefaultAction(action);
        else
            button->setCheckable(true);
        button->setIcon(qnSkin->icon("panel/pin.png", "panel/unpin.png"));
        button->setCached(true);
        setHelpTopic(button, Qn::MainWindow_Pin_Help);
        return button;
    }

    QGraphicsWidget *newSpacerWidget(qreal w, qreal h) {
        GraphicsWidget *result = new GraphicsWidget();
        result->setMinimumSize(QSizeF(w, h));
        result->setMaximumSize(result->minimumSize());
        return result;
    }

    class QnResizerWidget: public GraphicsWidget {
        typedef GraphicsWidget base_type;

    public:
        QnResizerWidget(Qt::Orientation orientation, QGraphicsItem *parent = NULL, Qt::WindowFlags wFlags = 0):
            base_type(parent, wFlags),
            m_orientation(orientation)
        {
            setAcceptedMouseButtons(Qt::LeftButton);
            setAcceptHoverEvents(false);
            setFlag(ItemHasNoContents, true);
            setHandlingFlag(ItemHandlesMovement, true);
            setEnabled(true);
        }

        void setEnabled(bool enabled)
        {
            setFlag(ItemIsMovable, enabled);
            if (enabled)
                setCursor(m_orientation == Qt::Vertical ? Qt::SizeVerCursor : Qt::SizeHorCursor);
            else
                setCursor(QCursor());
        }
    private:
        Qt::Orientation m_orientation;
    };

    class QnTabBarGraphicsProxyWidget: public QGraphicsProxyWidget {
        typedef QGraphicsProxyWidget base_type;
    public:
        QnTabBarGraphicsProxyWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): base_type(parent, windowFlags) {}

    protected:
        virtual bool eventFilter(QObject *object, QEvent *event) override {
            if(object == widget()) {
                if(event->type() == QEvent::Move)
                    return false; /* Don't propagate moves. */

                if(event->type() == QEvent::UpdateRequest && isVisible())
                    widget()->setAttribute(Qt::WA_Mapped); /* This one gets cleared for no reason in some cases. We just hack it around. */
            }

            return base_type::eventFilter(object, event);
        }

        virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override {
            if(change == ItemPositionChange || change == ItemPositionHasChanged)
                return value; /* Don't propagate moves. */

            return base_type::itemChange(change, value);
        }
    };

    const qreal normalTreeOpacity = 0.85;
    const qreal hoverTreeOpacity = 0.95;
    const qreal normalTreeBackgroundOpacity = 0.5;
    const qreal hoverTreeBackgroundOpacity = 1.0;

    const qreal normalSliderOpacity = 0.5;
    const qreal hoverSliderOpacity = 0.95;

    const qreal normalTitleBackgroundOpacity = 0.5;
    const qreal hoverTitleBackgroundOpacity = 0.95;

    const qreal normalNotificationsOpacity = 0.85;
    const qreal hoverNotificationsOpacity = 0.95;
    const qreal normalNotificationsBackgroundOpacity = 0.5;
    const qreal hoverNotificationsBackgroundOpacity = 1.0;

    const qreal normalCalendarOpacity = 0.5;
    const qreal hoverCalendarOpacity = 0.95;

    const qreal normalSearchOpacity = 0.85;
    const qreal hoverSearchOpacity = 0.95;

    const qreal opaque = 1.0;
    const qreal hidden = 0.0;

    const int hideConstrolsTimeoutMSec = 2000;
    const int closeConstrolsTimeoutMSec = 2000;

    const int sliderAutoHideTimeoutMSec = 10000;

} // anonymous namespace

static QtMessageHandler previousMsgHandler = 0;
static QnDebugProxyLabel* debugLabel = 0;

static void uiMsgHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (previousMsgHandler) {
        previousMsgHandler(type, ctx, msg);
    } 
    if (!debugLabel)
        return;
    debugLabel->appendTextQueued(msg);
}


QnWorkbenchUi::QnWorkbenchUi(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_instrumentManager(display()->instrumentManager()),
    m_flags(0),
    m_treeVisible(false),
    m_titleUsed(false),
    m_titleVisible(false),
    m_sliderVisible(false),
    m_notificationsPinned(false),
    m_notificationsOpened(false),
    m_notificationsVisible(false),
    m_calendarOpened(false),
    m_calendarVisible(false),
    m_dayTimeOpened(false),
    m_windowButtonsUsed(true),
    m_searchVisible(false),
    m_searchOpened(false),
    m_ignoreClickEvent(false),
    m_inactive(false),
    m_inFreespace(false),
    m_ignoreSliderResizerGeometryChanges(false),
    m_ignoreSliderResizerGeometryLater(false),
    m_ignoreTreeResizerGeometryChanges(false),
    m_updateTreeResizerGeometryLater(false),
    m_sliderZoomingIn(false),
    m_sliderZoomingOut(false),
    m_lastThumbnailsHeight(48.0),

    m_notificationsBackgroundItem(NULL),
    m_notificationsItem(NULL),
    m_notificationsPinButton(NULL),
    m_notificationsShowButton(NULL),
    m_notificationsOpacityProcessor(NULL),
    m_notificationsHidingProcessor(NULL),
    m_notificationsShowingProcessor(NULL),
    m_notificationsXAnimator(NULL),
    m_notificationsOpacityAnimatorGroup(NULL),

    m_calendarPinButton(nullptr),
    m_dayTimeMinimizeButton(nullptr),
    m_inCalendarGeometryUpdate(false),
    m_inDayTimeGeometryUpdate(false),
    m_inSearchGeometryUpdate(false),

    m_calendarPinOffset(),
    m_dayTimeOffset()
{
    memset(m_widgetByRole, 0, sizeof(m_widgetByRole));

    QGraphicsLayout::setInstantInvalidatePropagation(true);

    /* Install and configure instruments. */
    m_fpsCountingInstrument = new FpsCountingInstrument(1000, this);
    m_controlsActivityInstrument = new ActivityListenerInstrument(true, hideConstrolsTimeoutMSec, this);

    m_instrumentManager->installInstrument(m_fpsCountingInstrument, InstallationMode::InstallBefore, display()->paintForwardingInstrument());
    m_instrumentManager->installInstrument(m_controlsActivityInstrument);

    connect(m_controlsActivityInstrument, &ActivityListenerInstrument::activityStopped,   this,     &QnWorkbenchUi::at_activityStopped);
    connect(m_controlsActivityInstrument, &ActivityListenerInstrument::activityResumed,   this,     &QnWorkbenchUi::at_activityStarted);
    connect(m_fpsCountingInstrument,    &FpsCountingInstrument::fpsChanged,               this,     [this](qreal fps) {
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

    /* Tree panel. */
    createTreeWidget();

    /* Title bar. */
    createTitleWidget();

    if (!(qnSettings->lightMode() & Qn::LightModeNoNotifications))
        createNotificationsWidget();

    /* Calendar. */
    createCalendarWidget();

    /* Navigation slider. */
    createSliderWidget();

    /* Bookmarks search line. */
    createSearchWidget();

    /* Debug overlay */
    //createDebugWidget();

    initGraphicsMessageBox();

    /* Connect to display. */
    display()->view()->addAction(action(Qn::FreespaceAction));
    connect(action(Qn::FreespaceAction),&QAction::triggered,                        this,           &QnWorkbenchUi::at_freespaceAction_triggered);
    connect(action(Qn::EffectiveMaximizeAction), &QAction::triggered,               this,           [this](){ if(m_inFreespace) at_freespaceAction_triggered();});
    connect(display(),                  &QnWorkbenchDisplay::viewportGrabbed,       this,           &QnWorkbenchUi::disableProxyUpdates);
    connect(display(),                  &QnWorkbenchDisplay::viewportUngrabbed,     this,           &QnWorkbenchUi::enableProxyUpdates);
    connect(display(),                  &QnWorkbenchDisplay::widgetChanged,         this,           &QnWorkbenchUi::at_display_widgetChanged);

    /* Init fields. */
    m_pinOffset = (24 - QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, NULL, NULL)) / 2.0;

    setFlags(HideWhenNormal | HideWhenZoomed | AdjustMargins);

    setSliderVisible(false, false);
    setTreeVisible(true, false);
    setTitleVisible(true, false);
    setTitleUsed(false);
    setNotificationsVisible(true, false);
    setCalendarOpened(false, false);
    setDayTimeWidgetOpened(false, false);
    setCalendarVisible(false);
    updateControlsVisibility(false);

    //TODO: #GDM #Common think about a refactoring
    bool treeOpened = qnSettings->isTreeOpened(); //quite a hack because m_treePinButton sets tree opened if it is pinned
    bool notificationsOpened = qnSettings->isNotificationsOpened(); //same shit
    m_treePinButton->setChecked(qnSettings->isTreePinned());
    if (m_notificationsPinButton)
        m_notificationsPinButton->setChecked(qnSettings->isNotificationsPinned());
    setTreeOpened(treeOpened, false);
    setTitleOpened(qnSettings->isTitleOpened(), false, false);
    setSliderOpened(qnSettings->isSliderOpened(), false, false);
    setNotificationsOpened(notificationsOpened, false);

    m_calendarPinButton->setChecked(qnSettings->isCalendarPinned());

    /* Set up title D&D. */
    DropInstrument *dropInstrument = new DropInstrument(true, context(), this);
    display()->instrumentManager()->installInstrument(dropInstrument);
    dropInstrument->setSurface(m_titleBackgroundItem);
}

QnWorkbenchUi::~QnWorkbenchUi() {
    debugLabel = 0;
    /* The disconnect call is needed so that our methods don't get triggered while
     * the ui machinery is shutting down. */
    disconnectAll();

    delete m_controlsWidget;
}

Qn::ActionScope QnWorkbenchUi::currentScope() const {
    QGraphicsItem *focusItem = display()->scene()->focusItem();
    if(focusItem == m_treeItem) {
        return Qn::TreeScope;
    } else if(focusItem == m_sliderItem) {
        return Qn::SliderScope;
    } else if(focusItem == m_tabBarItem) {
        return Qn::TitleBarScope;
    } else if(!focusItem || dynamic_cast<QnResourceWidget *>(focusItem)) {
        return Qn::SceneScope;
    } else {
        return Qn::MainScope;
    }
}

QnActionParameters QnWorkbenchUi::currentParameters(Qn::ActionScope scope) const {
    /* Get items. */
    switch(scope) {
    case Qn::TitleBarScope:
        return m_tabBarWidget->currentParameters(scope);
    case Qn::TreeScope:
        return m_treeWidget->currentParameters(scope);
    default:
        return QnActionParameters(currentTarget(scope));
    }
}

QVariant QnWorkbenchUi::currentTarget(Qn::ActionScope scope) const {
    /* Get items. */
    switch(scope) {
    case Qn::SliderScope:
        return QVariant::fromValue(navigator()->currentWidget());
    case Qn::SceneScope:
        return QVariant::fromValue(QnActionParameterTypes::widgets(display()->scene()->selectedItems()));
    default:
        return QVariant();
    }
}

void QnWorkbenchUi::setProxyUpdatesEnabled(bool updatesEnabled) {
    m_treeItem->setUpdatesEnabled(updatesEnabled);
}

void QnWorkbenchUi::setWindowButtonsUsed(bool windowButtonsUsed) {
    if(m_windowButtonsUsed == windowButtonsUsed)
        return;

    m_windowButtonsUsed = windowButtonsUsed;

    if(m_windowButtonsUsed) {
        m_titleRightButtonsLayout->addItem(m_windowButtonsWidget);
        m_windowButtonsWidget->setVisible(true);
    } else {
        m_titleRightButtonsLayout->removeItem(m_windowButtonsWidget);
        m_windowButtonsWidget->setVisible(false);
    }
}

void QnWorkbenchUi::updateControlsVisibility(bool animate) {    // TODO
    ensureAnimationAllowed(animate);

    if (qnSettings->isVideoWallMode()) {
        bool sliderVisible =
            navigator()->currentWidget() != NULL &&
            !(navigator()->currentWidget()->resource()->flags() & (Qn::still_image | Qn::server));

        setSliderVisible(sliderVisible, animate);
        setTreeVisible(false, false);
        setTitleVisible(false, false);
        setNotificationsVisible(false, false);
        setSearchVisible(false, false);
        return;
    }

    bool sliderVisible =
        navigator()->currentWidget() != NULL &&
        !(navigator()->currentWidget()->resource()->flags() & (Qn::still_image | Qn::server | Qn::videowall)) &&
        ((accessController()->globalPermissions() & Qn::GlobalViewArchivePermission) || !(navigator()->currentWidget()->resource()->flags() & Qn::live)) &&
        !action(Qn::ToggleTourModeAction)->isChecked();

    if(m_inactive) {
        bool hovered = isHovered();
        setSliderVisible(sliderVisible && hovered, animate);
        setTreeVisible(hovered, animate);
        setTitleVisible(hovered, animate);
        setNotificationsVisible(hovered, animate);
    } else {
        setSliderVisible(sliderVisible, animate);
        setTreeVisible(true, animate);
        setTitleVisible(true, animate);
        setNotificationsVisible(true, animate);
    }

    updateCalendarVisibility(animate);
    updateSearchVisibility(animate);
}

QMargins QnWorkbenchUi::calculateViewportMargins(qreal treeX, qreal treeW, qreal titleY, qreal titleH, qreal sliderY, qreal notificationsX) {
    QMargins result(
        isTreePinned() ? std::floor(qMax(0.0, treeX + treeW)) : 0.0,
        std::floor(qMax(0.0, titleY + titleH)),
        m_notificationsItem ? std::floor(qMax(0.0, m_notificationsPinned ? m_controlsWidgetRect.right() - notificationsX : 0.0)) : 0.0,
        std::floor(qMax(0.0, m_controlsWidgetRect.bottom() - sliderY))
    );

    if (result.left() + result.right() >= m_controlsWidgetRect.width()) {
        result.setLeft(0.0);
        result.setRight(0.0);
    }

    return result;

}

void QnWorkbenchUi::setFlags(Flags flags) {
    if(flags == m_flags)
        return;

    m_flags = flags;

    updateActivityInstrumentState();
    updateViewportMargins();
}

void QnWorkbenchUi::updateViewportMargins() {
    if(!(m_flags & AdjustMargins)) {
        display()->setViewportMargins(QMargins(0, 0, 0, 0));
    } else {
        display()->setViewportMargins(calculateViewportMargins(
            m_treeXAnimator->isRunning() ? m_treeXAnimator->targetValue().toReal() : m_treeItem->pos().x(),
            m_treeItem->size().width(),
            m_titleYAnimator->isRunning() ? m_titleYAnimator->targetValue().toReal() : m_titleItem->pos().y(),
            m_titleItem->size().height(),
            m_sliderYAnimator->isRunning() ? m_sliderYAnimator->targetValue().toReal() : m_sliderItem->pos().y(),
            m_notificationsItem ? m_notificationsXAnimator->isRunning() ? m_notificationsXAnimator->targetValue().toReal() : m_notificationsItem->pos().x() : 0.0
        ));
    }
}

void QnWorkbenchUi::updateActivityInstrumentState() {
    bool zoomed = m_widgetByRole[Qn::ZoomedRole] != NULL;

    if(zoomed) {
        m_controlsActivityInstrument->setEnabled(m_flags & HideWhenZoomed);
    } else {
        m_controlsActivityInstrument->setEnabled(m_flags & HideWhenNormal);
    }
}

bool QnWorkbenchUi::isHovered() const {
    return  m_sliderOpacityProcessor->isHovered() ||
            m_treeOpacityProcessor->isHovered() ||
            m_titleOpacityProcessor->isHovered() ||
            (m_notificationsOpacityProcessor && m_notificationsOpacityProcessor->isHovered()) || // in light mode it can be NULL
            m_calendarOpacityProcessor->isHovered()
            || m_searchOpacityProcessor->isHovered()
            ;
}

QnWorkbenchUi::Panels QnWorkbenchUi::openedPanels() const {
    return
        (isTreeOpened() ? TreePanel : NoPanel) |
        (isTitleOpened() ? TitlePanel : NoPanel) |
        (isSliderOpened() ? SliderPanel : NoPanel) |
        (isNotificationsOpened() ? NotificationsPanel : NoPanel);
}

void QnWorkbenchUi::setOpenedPanels(Panels panels, bool animate, bool save) {
    ensureAnimationAllowed(animate);

    setTreeOpened(panels & TreePanel, animate, save);
    setTitleOpened(panels & TitlePanel, animate, save);
    setSliderOpened(panels & SliderPanel, animate, save);
    setNotificationsOpened(panels & NotificationsPanel, animate, save);
}

void QnWorkbenchUi::initGraphicsMessageBox() {
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

bool QnWorkbenchUi::event(QEvent *event) {
    bool result = base_type::event(event);

    if(event->type() == QnEvent::WinSystemMenu) {
        if(m_mainMenuButton->isVisible())
            m_mainMenuButton->click();

        result = true;
    }

    return result;
}

void QnWorkbenchUi::tick(int deltaMSecs) {
    if(!m_sliderZoomingIn && !m_sliderZoomingOut)
        return;

    if(!display()->scene())
        return;

    QnTimeSlider *slider = m_sliderItem->timeSlider();

    QPointF pos;
    if(slider->windowStart() <= slider->sliderPosition() && slider->sliderPosition() <= slider->windowEnd()) {
        pos = slider->positionFromValue(slider->sliderPosition(), true);
    } else {
        pos = slider->rect().center();
    }

    QGraphicsSceneWheelEvent event(QEvent::GraphicsSceneWheel);
    event.setDelta(360 * 8 * (m_sliderZoomingIn ? deltaMSecs : -deltaMSecs) / 1000); /* 360 degrees per sec, x8 since delta is measured in in eighths (1/8s) of a degree. */
    event.setPos(pos);
    event.setScenePos(slider->mapToScene(pos));
    display()->scene()->sendEvent(slider, &event);
}

void QnWorkbenchUi::at_freespaceAction_triggered() {
    QAction *fullScreenAction = action(Qn::EffectiveMaximizeAction);

    bool isFullscreen = fullScreenAction->isChecked();

    if(!m_inFreespace)
        m_inFreespace = isFullscreen && !isTreeOpened() && !isTitleOpened() && !isNotificationsOpened() && !isSliderOpened();

    if(!m_inFreespace) {
        if(!isFullscreen)
            fullScreenAction->setChecked(true);

        setTreeOpened(false, isFullscreen, false);
        setTitleOpened(false, isFullscreen, false);
        setSliderOpened(false, isFullscreen, false);
        setNotificationsOpened(false, isFullscreen, false);

        updateViewportMargins(); /* This one is needed here so that fit-in-view operates on correct margins. */ // TODO: #Elric change code so that this call is not needed.
        action(Qn::FitInViewAction)->trigger();

        m_inFreespace = true;
    } else {
        setTreeOpened(qnSettings->isTreeOpened(), isFullscreen);
        setTitleOpened(qnSettings->isTitleOpened(), isFullscreen);
        setSliderOpened(qnSettings->isSliderOpened(), isFullscreen);
        setNotificationsOpened(qnSettings->isNotificationsOpened(), isFullscreen);

        updateViewportMargins(); /* This one is needed here so that fit-in-view operates on correct margins. */ // TODO: #Elric change code so that this call is not needed.
        action(Qn::FitInViewAction)->trigger();

        m_inFreespace = false;
    }
}

void QnWorkbenchUi::at_activityStopped() {
    m_inactive = true;

    updateControlsVisibility(true);

    foreach(QnResourceWidget *widget, display()->widgets())
        if(!(widget->options() & QnResourceWidget::DisplayInfo))
            widget->setOverlayVisible(false);
}

void QnWorkbenchUi::at_activityStarted() {
    m_inactive = false;

    updateControlsVisibility(true);

    foreach(QnResourceWidget *widget, display()->widgets())
        if(widget->isInfoVisible()) // TODO: #Elric wrong place?
            widget->setOverlayVisible(true);
}

void QnWorkbenchUi::at_display_widgetChanged(Qn::ItemRole role) {
    //QnResourceWidget *oldWidget = m_widgetByRole[role];
    bool alreadyZoomed = m_widgetByRole[role] != NULL;

    QnResourceWidget *newWidget = display()->widget(role);
    m_widgetByRole[role] = newWidget;

    /* Update activity listener instrument. */
    if(role == Qn::ZoomedRole) {
        updateActivityInstrumentState();
        updateViewportMargins();
    }

    if(role == Qn::ZoomedRole) {
        if(newWidget) {
            if (!alreadyZoomed)
                m_unzoomedOpenedPanels = openedPanels();
            setOpenedPanels(openedPanels() & SliderPanel, true, false); /* Leave slider open. */
        } else {
            /* User may have opened some panels while zoomed,
             * we want to leave them opened even if they were closed before. */
            setOpenedPanels(m_unzoomedOpenedPanels | openedPanels(), true, false);

            /* Viewport margins have changed, force fit-in-view. */
            display()->fitInView();
        }
    }

    if (qnSettings->isVideoWallMode()) {
        switch (role) {
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

void QnWorkbenchUi::ensureAnimationAllowed(bool &animate) {
    if (animate && (qnSettings->lightMode() & Qn::LightModeNoAnimation))
        animate = false;
}

#pragma region ControlsWidget

void QnWorkbenchUi::at_controlsWidget_geometryChanged() {
    QGraphicsWidget *controlsWidget = m_controlsWidget;
    QRectF rect = controlsWidget->rect();
    if(qFuzzyEquals(m_controlsWidgetRect, rect))
        return;
    QRectF oldRect = m_controlsWidgetRect;
    m_controlsWidgetRect = rect;

    /* We lay everything out manually. */

    m_sliderItem->setGeometry(QRectF(
        0.0,
        m_sliderItem->pos().y() - oldRect.height() + rect.height(),
        rect.width(),
        m_sliderItem->size().height()
    ));

    m_titleItem->setGeometry(QRectF(
        0.0,
        m_titleItem->pos().y(),
        rect.width(),
        m_titleItem->size().height()
    ));

    if (m_notificationsItem) {
        if (m_notificationsXAnimator->isRunning())
            m_notificationsXAnimator->stop();
        m_notificationsItem->setX(rect.right() + (m_notificationsOpened ? -m_notificationsItem->size().width() : 1.0 /* Just in case. */));
    }

    updateTreeGeometry();
    updateNotificationsGeometry();
    updateFpsGeometry();
}

void QnWorkbenchUi::createControlsWidget() {
    m_controlsWidget = new QnUiElementsWidget();
    m_controlsWidget->setAcceptedMouseButtons(0);
    display()->scene()->addItem(m_controlsWidget);
    display()->setLayer(m_controlsWidget, Qn::UiLayer);

    QnSingleEventSignalizer *deactivationSignalizer = new QnSingleEventSignalizer(this);
    deactivationSignalizer->setEventType(QEvent::WindowDeactivate);
    m_controlsWidget->installEventFilter(deactivationSignalizer);

    /* Reactivate controls widget if it is deactivated. */
    connect(deactivationSignalizer,     &QnAbstractEventSignalizer::activated,  this,   [this](){display()->scene()->setActiveWindow(m_controlsWidget);});

    connect(m_controlsWidget,           &QGraphicsWidget::geometryChanged,      this,   &QnWorkbenchUi::at_controlsWidget_geometryChanged);
}

#pragma endregion Main controls widget methods

#pragma region TreeWidget

void QnWorkbenchUi::setTreeShowButtonUsed(bool used) {
    if(used) {
        m_treeShowButton->setAcceptedMouseButtons(Qt::LeftButton);
    } else {
        m_treeShowButton->setAcceptedMouseButtons(0);
    }
}

void QnWorkbenchUi::setTreeVisible(bool visible, bool animate) {
    ensureAnimationAllowed(animate);

    bool changed = m_treeVisible != visible;

    m_treeVisible = visible;

    updateTreeOpacity(animate);
    if(changed)
        updateTreeGeometry();
}

void QnWorkbenchUi::setTreeOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate) {
    ensureAnimationAllowed(animate);

    if(animate) {
        m_treeOpacityAnimatorGroup->pause();
        opacityAnimator(m_treeItem)->setTargetValue(foregroundOpacity);
        opacityAnimator(m_treePinButton)->setTargetValue(foregroundOpacity);
        opacityAnimator(m_treeBackgroundItem)->setTargetValue(backgroundOpacity);
        opacityAnimator(m_treeShowButton)->setTargetValue(backgroundOpacity);
        m_treeOpacityAnimatorGroup->start();
    } else {
        m_treeOpacityAnimatorGroup->stop();
        m_treeItem->setOpacity(foregroundOpacity);
        m_treePinButton->setOpacity(foregroundOpacity);
        m_treeBackgroundItem->setOpacity(backgroundOpacity);
        m_treeShowButton->setOpacity(backgroundOpacity);
    }

    m_treeResizerWidget->setVisible(!qFuzzyIsNull(foregroundOpacity));
}

void QnWorkbenchUi::updateTreeOpacity(bool animate) {
    if (qnSettings->lightMode() & Qn::LightModeNoOpacity) {
        qreal opacity = m_treeVisible ? opaque : hidden;
        setTreeOpacity(opacity, opacity, false);
        return;
    }

    if(!m_treeVisible) {
        setTreeOpacity(0.0, 0.0, animate);
    } else {
        if(m_treeOpacityProcessor->isHovered()) {
            setTreeOpacity(hoverTreeOpacity, hoverTreeBackgroundOpacity, animate);
        } else {
            setTreeOpacity(normalTreeOpacity, normalTreeBackgroundOpacity, animate);
        }
    }
}

bool QnWorkbenchUi::isTreePinned() const {
    return action(Qn::PinTreeAction)->isChecked();
}

bool QnWorkbenchUi::isCalendarPinned() const
{
    return action(Qn::PinCalendarAction)->isChecked();
}

bool QnWorkbenchUi::isTreeOpened() const {
    return action(Qn::ToggleTreeAction)->isChecked();
}

void QnWorkbenchUi::setTreeOpened(bool opened, bool animate, bool save) {
    ensureAnimationAllowed(animate);

    m_inFreespace = false;

    m_treeShowingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(Qn::ToggleTreeAction)->setChecked(opened);

    qreal newX = opened ? 0.0 : -m_treeItem->size().width() - 1.0 /* Just in case. */;
    if (animate) {
        m_treeXAnimator->animateTo(newX);
    } else {
        m_treeXAnimator->stop();
        m_treeItem->setX(newX);
    }

    if (save)
        qnSettings->setTreeOpened(opened);

    static_cast<QnResizerWidget*>(m_treeResizerWidget)->setEnabled(opened);
}

QRectF QnWorkbenchUi::updatedTreeGeometry(const QRectF &treeGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry) {
    QPointF pos(
        treeGeometry.x(),
        ((!m_titleVisible || !m_titleUsed) && m_treeVisible) ? 30.0 : qMax(titleGeometry.bottom() + 30.0, 30.0)
        );
    QSizeF size(
        treeGeometry.width(),
        ((!m_sliderVisible && m_treeVisible) ? m_controlsWidgetRect.bottom() - 30.0 : qMin(sliderGeometry.y() - 30.0, m_controlsWidgetRect.bottom() - 30.0)) - pos.y()
        );
    return QRectF(pos, size);
}

void QnWorkbenchUi::updateTreeGeometry() {
    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedTreeGeometry(m_treeItem->geometry(), m_titleItem->geometry(), m_sliderItem->geometry());
    m_treeItem->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_treeItem->setPos(geometry.topLeft());

    /* Whether actual size change should be deferred. */
    bool defer = false;

    /* Calculate slider target position. */
    QPointF sliderPos;
    if(!m_sliderVisible && m_treeVisible) {
        sliderPos = QPointF(m_sliderItem->pos().x(), m_controlsWidgetRect.bottom());
    } else if(m_sliderYAnimator->isRunning()) {
        sliderPos = QPointF(m_sliderItem->pos().x(), m_sliderYAnimator->targetValue().toReal());
        defer |= !qFuzzyEquals(sliderPos, m_sliderItem->pos()); /* If animation is running, then geometry sync should be deferred. */
    } else {
        sliderPos = m_sliderItem->pos();
    }

    /* Calculate title target position. */
    QPointF titlePos;
    if((!m_titleVisible || !m_titleUsed) && m_treeVisible) {
        titlePos = QPointF(m_titleItem->pos().x(), -m_titleItem->size().height());
    } else if(m_titleYAnimator->isRunning()) {
        titlePos = QPointF(m_titleItem->pos().x(), m_titleYAnimator->targetValue().toReal());
        defer |= !qFuzzyEquals(titlePos, m_titleItem->pos());
    } else {
        titlePos = m_titleItem->pos();
    }

    /* Calculate target geometry. */
    geometry = updatedTreeGeometry(m_treeItem->geometry(), QRectF(titlePos, m_titleItem->size()), QRectF(sliderPos, m_sliderItem->size()));
    if(qFuzzyEquals(geometry, m_treeItem->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if(defer && geometry.height() < m_treeItem->size().height())
        return;

    m_treeItem->resize(geometry.size());
}

void QnWorkbenchUi::updateTreeResizerGeometry() {
    if(m_updateTreeResizerGeometryLater) {
        QTimer::singleShot(1, this, SLOT(updateTreeResizerGeometry()));
        return;
    }

    QRectF treeRect = m_treeItem->rect();

    QRectF treeResizerGeometry = QRectF(
        m_controlsWidget->mapFromItem(m_treeItem, treeRect.topRight()),
        m_controlsWidget->mapFromItem(m_treeItem, treeRect.bottomRight())
        );

    treeResizerGeometry.moveTo(treeResizerGeometry.topRight());
    treeResizerGeometry.setWidth(8);

    if(!qFuzzyEquals(treeResizerGeometry, m_treeResizerWidget->geometry())) {
        QN_SCOPED_VALUE_ROLLBACK(&m_updateTreeResizerGeometryLater, true);

        m_treeResizerWidget->setGeometry(treeResizerGeometry);

        /* This one is needed here as we're in a handler and thus geometry change doesn't adjust position =(. */
        m_treeResizerWidget->setPos(treeResizerGeometry.topLeft());  // TODO: #Elric remove this ugly hack.
    }
}

void QnWorkbenchUi::at_treeWidget_activated(const QnResourcePtr &resource) {
    // user resources cannot be dropped on the scene
    if(!resource || resource.dynamicCast<QnUserResource>())
        return;

    menu()->trigger(Qn::DropResourcesAction, resource);
}

void QnWorkbenchUi::at_treeItem_paintGeometryChanged() {
    QRectF paintGeometry = m_treeItem->paintGeometry();

    /* Don't hide tree item here. It will repaint itself when shown, which will
     * degrade performance. */

    m_treeBackgroundItem->setGeometry(paintGeometry);
    m_treeShowButton->setPos(QPointF(
        qMax(m_controlsWidgetRect.left(), paintGeometry.right()),
        (paintGeometry.top() + paintGeometry.bottom() - m_treeShowButton->size().height()) / 2
    ));
    m_treePinButton->setPos(QPointF(
        paintGeometry.right() - m_treePinButton->size().width() - m_pinOffset,
        paintGeometry.top() + m_pinOffset
    ));

    updateTreeResizerGeometry();
    updateViewportMargins();
}

void QnWorkbenchUi::at_treeResizerWidget_geometryChanged() {
    if(m_ignoreTreeResizerGeometryChanges)
        return;

    QRectF resizerGeometry = m_treeResizerWidget->geometry();
    if (!resizerGeometry.isValid()) {
        updateTreeResizerGeometry();
        return;
    }

    QRectF treeGeometry = m_treeItem->geometry();

    qreal targetWidth = m_treeResizerWidget->geometry().left() - treeGeometry.left();
    qreal minWidth = m_treeItem->effectiveSizeHint(Qt::MinimumSize).width();
    qreal maxWidth = m_controlsWidget->geometry().width() / 2;

    targetWidth = qBound(minWidth, targetWidth, maxWidth);

    if(!qFuzzyCompare(treeGeometry.width(), targetWidth)) {
        qnSettings->setTreeWidth(qRound(targetWidth));
        treeGeometry.setWidth(targetWidth);
        treeGeometry.setLeft(0);

        QN_SCOPED_VALUE_ROLLBACK(&m_ignoreTreeResizerGeometryChanges, true);
        m_treeWidget->resize(targetWidth, m_treeWidget->height());
        m_treeItem->setPaintGeometry(treeGeometry);
    }

    updateTreeResizerGeometry();
}

void QnWorkbenchUi::at_treeShowingProcessor_hoverEntered() {
    if(!isTreePinned() && !isTreeOpened()) {
        setTreeOpened(true);

        /* So that the click that may follow won't hide it. */
        setTreeShowButtonUsed(false);
        QTimer::singleShot(300, this, SLOT(setTreeShowButtonUsed()));
    }

    m_treeHidingProcessor->forceHoverEnter();
    m_treeOpacityProcessor->forceHoverEnter();
}

void QnWorkbenchUi::at_pinTreeAction_toggled(bool checked) {
    if(checked)
        setTreeOpened(true);

    updateViewportMargins();

    qnSettings->setTreePinned(checked);
}

void QnWorkbenchUi::createTreeWidget() {
    m_treeWidget = new QnResourceBrowserWidget(NULL, context());
    m_treeWidget->setAttribute(Qt::WA_TranslucentBackground);

    QPalette defaultPalette = m_treeWidget->palette();
    setPaletteColor(m_treeWidget, QPalette::Window, Qt::transparent);
    setPaletteColor(m_treeWidget, QPalette::Base, Qt::transparent);
    setPaletteColor(m_treeWidget->typeComboBox(), QPalette::Base, defaultPalette.color(QPalette::Base));
    m_treeWidget->resize(qnSettings->treeWidth(), 0);

    m_treeBackgroundItem = new QnControlBackgroundWidget(Qn::LeftBorder, m_controlsWidget);

    m_treeItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_treeItem->setWidget(m_treeWidget);
    m_treeWidget->installEventFilter(m_treeItem);
    m_treeWidget->setToolTipParent(m_treeItem);
    m_treeItem->setFocusPolicy(Qt::StrongFocus);
    m_treeItem->setProperty(Qn::NoHandScrollOver, true);

    m_treePinButton = newPinButton(m_controlsWidget, action(Qn::PinTreeAction));
    m_treePinButton->setFocusProxy(m_treeItem);

    m_treeShowButton = newShowHideButton(m_controlsWidget, action(Qn::ToggleTreeAction));
    m_treeShowButton->setFocusProxy(m_treeItem);

    m_treeResizerWidget = new QnResizerWidget(Qt::Horizontal, m_controlsWidget);
    m_treeResizerWidget->setProperty(Qn::NoHandScrollOver, true);
    m_treeResizerWidget->stackBefore(m_treeShowButton);
    m_treeItem->stackBefore(m_treeResizerWidget);
    
    m_treeOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_treeOpacityProcessor->addTargetItem(m_treeItem);
    m_treeOpacityProcessor->addTargetItem(m_treeShowButton);

    m_treeHidingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_treeHidingProcessor->addTargetItem(m_treeItem);
    m_treeHidingProcessor->addTargetItem(m_treeShowButton);
    m_treeHidingProcessor->addTargetItem(m_treeResizerWidget);
    m_treeHidingProcessor->setHoverLeaveDelay(closeConstrolsTimeoutMSec);
    m_treeHidingProcessor->setFocusLeaveDelay(closeConstrolsTimeoutMSec);

    m_treeShowingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_treeShowingProcessor->addTargetItem(m_treeShowButton);
    m_treeShowingProcessor->setHoverEnterDelay(250);

    m_treeXAnimator = new VariantAnimator(this);
    m_treeXAnimator->setTimer(m_instrumentManager->animationTimer());
    m_treeXAnimator->setTargetObject(m_treeItem);
    m_treeXAnimator->setAccessor(new PropertyAccessor("x"));
    m_treeXAnimator->setSpeed(m_treeItem->size().width() * 2.0);
    m_treeXAnimator->setTimeLimit(500);

    m_treeOpacityAnimatorGroup = new AnimatorGroup(this);
    m_treeOpacityAnimatorGroup->setTimer(m_instrumentManager->animationTimer());
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treeItem));
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treeBackgroundItem)); /* Speed of 1.0 is OK here. */
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treeShowButton));
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treePinButton));


    {
        /* Do not auto-hide tree if we have opened context menu. */
        auto connectTreeHidingProcessor = [this] {
            connect(m_treeHidingProcessor, &HoverFocusProcessor::hoverFocusLeft, this, [this](){
                if(!isTreePinned())
                    setTreeOpened(false);
            });
        };

        QnSingleEventSignalizer *treeMenuSignalizer = new QnSingleEventSignalizer(this);
        treeMenuSignalizer->setEventType(QEvent::GraphicsSceneContextMenu);
        m_treeItem->installEventFilter(treeMenuSignalizer);
        connect(treeMenuSignalizer,         &QnAbstractEventSignalizer::activated,  this,   [this, connectTreeHidingProcessor] {
            if (isTreePinned() || !isTreeOpened())
                return;

            disconnect(m_treeHidingProcessor,  &HoverFocusProcessor::hoverFocusLeft, this, NULL);

            QnCounter* counter = new QnCounter(1, this);
            connect(menu(), &QnActionManager::menuAboutToShow, counter, &QnCounter::increment);
            connect(menu(), &QnActionManager::menuAboutToHide, counter, &QnCounter::decrement);
            connect(counter, &QnCounter::reachedZero, this, [this, connectTreeHidingProcessor]{
                connectTreeHidingProcessor();
                m_treeHidingProcessor->forceFocusLeave();
            });
            connect(counter, &QnCounter::reachedZero, counter, &QObject::deleteLater);

            /* Make sure counter will be triggered even if no menu created. */
            QTimer* nullMenuTimer = new QTimer(this);
            nullMenuTimer->setSingleShot(true);
            nullMenuTimer->setInterval(500);
            connect(nullMenuTimer, &QTimer::timeout, counter, &QnCounter::decrement);
            connect(nullMenuTimer, &QTimer::timeout, nullMenuTimer, &QObject::deleteLater);
            nullMenuTimer->start();
        });
        connectTreeHidingProcessor();
    }

    connect(m_treeWidget,               &QnResourceBrowserWidget::selectionChanged, action(Qn::SelectionChangeAction),  &QAction::trigger);
    connect(m_treeWidget,               &QnResourceBrowserWidget::activated,        this,                               &QnWorkbenchUi::at_treeWidget_activated);
    connect(m_treeOpacityProcessor,     &HoverFocusProcessor::hoverLeft,            this,                               &QnWorkbenchUi::updateTreeOpacityAnimated);
    connect(m_treeOpacityProcessor,     &HoverFocusProcessor::hoverEntered,         this,                               &QnWorkbenchUi::updateTreeOpacityAnimated);
    connect(m_treeOpacityProcessor,     &HoverFocusProcessor::hoverEntered,         this,                               &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_treeOpacityProcessor,     &HoverFocusProcessor::hoverLeft,            this,                               &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_treeShowingProcessor,     &HoverFocusProcessor::hoverEntered,         this,                               &QnWorkbenchUi::at_treeShowingProcessor_hoverEntered);
    connect(m_treeItem,                 &QnMaskedProxyWidget::paintRectChanged,     this,                               &QnWorkbenchUi::at_treeItem_paintGeometryChanged);
    connect(m_treeItem,                 &QGraphicsWidget::geometryChanged,          this,                               &QnWorkbenchUi::at_treeItem_paintGeometryChanged);
    connect(m_treeResizerWidget,        &QGraphicsWidget::geometryChanged,          this,                               &QnWorkbenchUi::at_treeResizerWidget_geometryChanged, Qt::QueuedConnection);
    connect(action(Qn::ToggleTreeAction),       &QAction::toggled,                  this,                               [this](bool checked){ if (!m_ignoreClickEvent) setTreeOpened(checked);});
    connect(action(Qn::PinTreeAction),          &QAction::toggled,                  this,                               &QnWorkbenchUi::at_pinTreeAction_toggled);
    connect(action(Qn::PinNotificationsAction), &QAction::toggled,                  this,                               &QnWorkbenchUi::at_pinNotificationsAction_toggled);
    connect(action(Qn::PinCalendarAction),      &QAction::toggled,                  QnClientSettings::instance(),       &QnClientSettings::setCalendarPinned);
}

#pragma endregion Tree widget methods

#pragma region TitleWidget

void QnWorkbenchUi::setTitleUsed(bool used) {
    m_titleItem->setVisible(used);
    m_titleBackgroundItem->setVisible(used);
    m_titleShowButton->setVisible(used);

    if(used) {
        m_titleUsed = used;

        setTitleOpened(isTitleOpened(), false, false);

        at_titleItem_geometryChanged();

        /* For reasons unknown, tab bar's size gets messed up when it is shown
         * after new items are added to it. Re-embedding helps, probably there is
         * a better workaround. */
        QTabBar *widget = checked_cast<QTabBar *>(m_tabBarItem->widget());
        m_tabBarItem->setWidget(NULL);
        m_tabBarItem->setWidget(widget);

        /* There are cases where even re-embedding doesn't help.
         * So we cheat even more, forcing the tab bar to refresh. */
        QTabBar::Shape shape = widget->shape();
        widget->setShape(QTabBar::TriangularWest);
        widget->setShape(shape);
    } else {
        m_titleItem->setPos(0.0, -m_titleItem->size().height() - 1.0);

        m_titleUsed = used;
    }
}

void QnWorkbenchUi::setTitleOpened(bool opened, bool animate, bool save) {
    ensureAnimationAllowed(animate);

    m_inFreespace = false;

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(Qn::ToggleTitleBarAction)->setChecked(opened);

    if (save)
        qnSettings->setTitleOpened(opened);

    if(!m_titleUsed)
        return;

    qreal newY = opened ? 0.0 : -m_titleItem->size().height() - 1.0;
    if (animate) {
        m_titleYAnimator->animateTo(newY);
    } else {
        m_titleYAnimator->stop();
        m_titleItem->setY(newY);
    }
}

void QnWorkbenchUi::setTitleVisible(bool visible, bool animate) {
    ensureAnimationAllowed(animate);

    bool changed = m_titleVisible != visible;

    m_titleVisible = visible;

    updateTitleOpacity(animate);
    if(changed) {
        updateTreeGeometry();
        updateNotificationsGeometry();
    }
}

bool QnWorkbenchUi::isTitleOpened() const {
    return action(Qn::ToggleTitleBarAction)->isChecked();
}

void QnWorkbenchUi::setTitleOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate) {
    ensureAnimationAllowed(animate);

    if(animate) {
        m_titleOpacityAnimatorGroup->pause();
        opacityAnimator(m_titleItem)->setTargetValue(foregroundOpacity);
        opacityAnimator(m_titleBackgroundItem)->setTargetValue(backgroundOpacity);
        opacityAnimator(m_titleShowButton)->setTargetValue(backgroundOpacity);
        m_titleOpacityAnimatorGroup->start();
    } else {
        m_titleOpacityAnimatorGroup->stop();
        m_titleItem->setOpacity(foregroundOpacity);
        m_titleBackgroundItem->setOpacity(backgroundOpacity);
        m_titleShowButton->setOpacity(backgroundOpacity);
    }
}

void QnWorkbenchUi::updateTitleOpacity(bool animate) {
    if (qnSettings->lightMode() & Qn::LightModeNoOpacity) {
        qreal opacity = m_titleVisible ? opaque : hidden;
        setTitleOpacity(opacity, opacity, false);
        return;
    }

    if(!m_titleVisible) {
        setTitleOpacity(0.0, 0.0, animate);
    } else {
        if(m_titleOpacityProcessor->isHovered()) {
            setTitleOpacity(1.0, hoverTitleBackgroundOpacity, animate);
        } else {
            setTitleOpacity(1.0, normalTitleBackgroundOpacity, animate);
        }
    }
}

void QnWorkbenchUi::at_titleItem_geometryChanged() {
    if(!m_titleUsed)
        return;

    updateTreeGeometry();
    updateNotificationsGeometry();
    updateFpsGeometry();

    QRectF geometry = m_titleItem->geometry();

    m_titleBackgroundItem->setGeometry(geometry);

    m_titleShowButton->setPos(QPointF(
        (geometry.left() + geometry.right() - m_titleShowButton->size().height()) / 2,
        qMax(m_controlsWidget->rect().top(), geometry.bottom())
        ));
}

void QnWorkbenchUi::at_titleItem_contextMenuRequested(QObject *, QEvent *event) {
    m_tabBarItem->setFocus();

    QGraphicsSceneContextMenuEvent *menuEvent = static_cast<QGraphicsSceneContextMenuEvent *>(event);

    /* Redirect context menu event to tab bar. */
    QPointF pos = menuEvent->pos();
    menuEvent->setPos(m_tabBarItem->mapFromItem(m_titleItem, pos));
    display()->scene()->sendEvent(m_tabBarItem, event);
    menuEvent->setPos(pos);
}

void QnWorkbenchUi::createTitleWidget() {
    m_titleBackgroundItem = new QnControlBackgroundWidget(Qn::TopBorder, m_controlsWidget);

    m_titleItem = new QnClickableWidget(m_controlsWidget);
    m_titleItem->setPos(0.0, 0.0);
    m_titleItem->setClickableButtons(Qt::LeftButton);
    m_titleItem->setProperty(Qn::NoHandScrollOver, true);

    QnSingleEventSignalizer *titleMenuSignalizer = new QnSingleEventSignalizer(this);
    titleMenuSignalizer->setEventType(QEvent::GraphicsSceneContextMenu);
    m_titleItem->installEventFilter(titleMenuSignalizer);

    /* Note: using QnGeometryGraphicsProxyWidget here fixes the bug #2330 */
    m_tabBarItem = new QnTabBarGraphicsProxyWidget(m_controlsWidget);
    m_tabBarItem->setCacheMode(QGraphicsItem::ItemCoordinateCache);

    m_tabBarWidget = new QnLayoutTabBar(NULL, context());
    m_tabBarWidget->setAttribute(Qt::WA_TranslucentBackground);
    m_tabBarItem->setWidget(m_tabBarWidget);
    m_tabBarWidget->installEventFilter(m_tabBarItem);
    connect(m_tabBarWidget, &QnLayoutTabBar::tabTextChanged, this, [this](){ m_titleItem->layout()->updateGeometry(); });

    m_mainMenuButton = newActionButton(action(Qn::MainMenuAction), 1.5, Qn::MainWindow_TitleBar_MainMenu_Help);

    QGraphicsLinearLayout * windowButtonsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    windowButtonsLayout->setContentsMargins(0, 0, 0, 0);
    windowButtonsLayout->setSpacing(2);
    windowButtonsLayout->addItem(newSpacerWidget(6.0, 6.0));
    windowButtonsLayout->addItem(newActionButton(action(Qn::WhatsThisAction)));
    windowButtonsLayout->addItem(newActionButton(action(Qn::MinimizeAction)));
    windowButtonsLayout->addItem(newActionButton(action(Qn::EffectiveMaximizeAction)
        , kDefaultSizeMultiplier, Qn::MainWindow_Fullscreen_Help));
    windowButtonsLayout->addItem(newActionButton(action(Qn::ExitAction)));

    m_windowButtonsWidget = new GraphicsWidget();
    m_windowButtonsWidget->setLayout(windowButtonsLayout);

    QGraphicsLinearLayout *titleLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    titleLayout->setSpacing(2);
    titleLayout->setContentsMargins(4, 0, 4, 0);
    QGraphicsLinearLayout *titleLeftButtonsLayout = new QGraphicsLinearLayout();
    titleLeftButtonsLayout->setSpacing(2);
    titleLeftButtonsLayout->setContentsMargins(0, 0, 0, 0);
    titleLeftButtonsLayout->addItem(m_mainMenuButton);
    titleLayout->addItem(titleLeftButtonsLayout);
    titleLayout->addItem(m_tabBarItem);
    titleLayout->setAlignment(m_tabBarItem, Qt::AlignBottom);
    m_titleRightButtonsLayout = new QGraphicsLinearLayout();
    m_titleRightButtonsLayout->setSpacing(2);
    m_titleRightButtonsLayout->setContentsMargins(0, 4, 0, 0);
    m_titleRightButtonsLayout->addItem(newActionButton(action(Qn::OpenNewTabAction)
        , kDefaultSizeMultiplier, Qn::MainWindow_TitleBar_NewLayout_Help));
    m_titleRightButtonsLayout->addItem(newActionButton(action(Qn::OpenCurrentUserLayoutMenu)));
    m_titleRightButtonsLayout->addStretch(0x1000);
    if (QnScreenRecorder::isSupported())
    {
        m_titleRightButtonsLayout->addItem(newActionButton(action(Qn::ToggleScreenRecordingAction)
        , kDefaultSizeMultiplier, Qn::MainWindow_ScreenRecording_Help));
    }
    m_titleRightButtonsLayout->addItem(newActionButton(action(Qn::OpenLoginDialogAction)));
    m_titleRightButtonsLayout->addItem(m_windowButtonsWidget);
    titleLayout->addItem(m_titleRightButtonsLayout);
    m_titleItem->setLayout(titleLayout);
    titleLayout->activate(); /* So that it would set title's size. */

    m_titleShowButton = newShowHideButton(m_controlsWidget, action(Qn::ToggleTitleBarAction));
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
    m_titleOpacityAnimatorGroup->addAnimator(opacityAnimator(m_titleBackgroundItem)); /* Speed of 1.0 is OK here. */
    m_titleOpacityAnimatorGroup->addAnimator(opacityAnimator(m_titleShowButton));

    connect(m_tabBarWidget,             &QnLayoutTabBar::closeRequested,        this,   [this](QnWorkbenchLayout *layout){ menu()->trigger(Qn::CloseLayoutAction, QnWorkbenchLayoutList() << layout);});
    connect(m_titleOpacityProcessor,    &HoverFocusProcessor::hoverEntered,     this,   &QnWorkbenchUi::updateTitleOpacityAnimated);
    connect(m_titleOpacityProcessor,    &HoverFocusProcessor::hoverLeft,        this,   &QnWorkbenchUi::updateTitleOpacityAnimated);
    connect(m_titleOpacityProcessor,    &HoverFocusProcessor::hoverEntered,     this,   &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_titleOpacityProcessor,    &HoverFocusProcessor::hoverLeft,        this,   &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_titleItem,                &QGraphicsWidget::geometryChanged,      this,   &QnWorkbenchUi::at_titleItem_geometryChanged);
#ifndef Q_OS_MACX
    connect(m_tabBarWidget,             &QnLayoutTabBar::tabCloseRequested,     m_titleItem,    &QnClickableWidget::skipDoubleClick);
    connect(m_tabBarWidget,             &QnLayoutTabBar::currentChanged,        m_titleItem,    &QnClickableWidget::skipDoubleClick);
    connect(m_titleItem,                &QnClickableWidget::doubleClicked,      action(Qn::EffectiveMaximizeAction), &QAction::toggle);
#endif
    connect(titleMenuSignalizer,        &QnAbstractEventSignalizer::activated,  this,   &QnWorkbenchUi::at_titleItem_contextMenuRequested);
    connect(action(Qn::ToggleTitleBarAction), &QAction::toggled,                this,   [this](bool checked){ if (!m_ignoreClickEvent) setTitleOpened(checked);});
}

#pragma endregion Title methods

#pragma region NotificationsWidget

bool QnWorkbenchUi::isNotificationsOpened() const {
    return m_notificationsOpened;
}

void QnWorkbenchUi::setNotificationsOpened(bool opened, bool animate, bool save) {
    if (!m_notificationsItem)
        return;

    ensureAnimationAllowed(animate);

    m_inFreespace = false;

    m_notificationsShowingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    m_notificationsOpened = opened;

    qreal newX = m_controlsWidgetRect.right() + (opened ? -m_notificationsItem->size().width() : 1.0 /* Just in case. */);
    if (animate) {
        m_notificationsXAnimator->setSpeed(m_notificationsItem->size().width() * 2.0);
        m_notificationsXAnimator->animateTo(newX);
    } else {
        m_notificationsXAnimator->stop();
        m_notificationsItem->setX(newX);
    }

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    m_notificationsShowButton->setChecked(opened);

    if (save)
        qnSettings->setNotificationsOpened(opened);
}

void QnWorkbenchUi::setNotificationsShowButtonUsed(bool used) {
    if(used) {
        m_notificationsShowButton->setAcceptedMouseButtons(Qt::LeftButton);
    } else {
        m_notificationsShowButton->setAcceptedMouseButtons(0);
    }
}

void QnWorkbenchUi::setNotificationsVisible(bool visible, bool animate) {
    ensureAnimationAllowed(animate);

    if (!m_notificationsItem)
        return;

    bool changed = m_notificationsVisible != visible;

    m_notificationsVisible = visible;

    updateNotificationsOpacity(animate);
    if(changed)
        updateNotificationsGeometry();
}

void QnWorkbenchUi::setNotificationsOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate) {
    ensureAnimationAllowed(animate);

    if (!m_notificationsItem)
        return;

    if(animate) {
        m_notificationsOpacityAnimatorGroup->pause();
        opacityAnimator(m_notificationsItem)->setTargetValue(foregroundOpacity);
        opacityAnimator(m_notificationsPinButton)->setTargetValue(foregroundOpacity);
        opacityAnimator(m_notificationsBackgroundItem)->setTargetValue(backgroundOpacity);
        opacityAnimator(m_notificationsShowButton)->setTargetValue(backgroundOpacity);
        m_notificationsOpacityAnimatorGroup->start();
    } else {
        m_notificationsOpacityAnimatorGroup->stop();
        m_notificationsItem->setOpacity(foregroundOpacity);
        m_notificationsPinButton->setOpacity(foregroundOpacity);
        m_notificationsBackgroundItem->setOpacity(backgroundOpacity);
        m_notificationsShowButton->setOpacity(backgroundOpacity);
    }
}

void QnWorkbenchUi::updateNotificationsOpacity(bool animate) {
    if (!m_notificationsItem)
        return;

    if(!m_notificationsVisible) {
        setNotificationsOpacity(0.0, 0.0, animate);
    } else {
        if(m_notificationsOpacityProcessor->isHovered()) {
            setNotificationsOpacity(hoverNotificationsOpacity, hoverNotificationsBackgroundOpacity, animate);
        } else {
            setNotificationsOpacity(normalNotificationsOpacity, normalNotificationsBackgroundOpacity, animate);
        }
    }
}

QRectF QnWorkbenchUi::updatedNotificationsGeometry(const QRectF &notificationsGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry, const QRectF &calendarGeometry, const QRectF &dayTimeGeometry, qreal *maxHeight) {
    QPointF pos(
        notificationsGeometry.x(),
        ((!m_titleVisible || !m_titleUsed) && m_notificationsVisible) ? 30.0 : qMax(titleGeometry.bottom() + 30.0, 30.0)
        );

    *maxHeight = qMin(
        m_sliderVisible ? sliderGeometry.y() - 30.0 : m_controlsWidgetRect.bottom() - 30.0,
        m_calendarVisible ? qMin(calendarGeometry.y(), dayTimeGeometry.y()) - 30.0 : m_controlsWidgetRect.bottom() - 30.0
        ) - pos.y();
    qreal preferredHeight = m_notificationsItem->preferredHeight();
    QSizeF size(notificationsGeometry.width(), qMin(*maxHeight, preferredHeight));
    return QRectF(pos, size);
}

void QnWorkbenchUi::updateNotificationsGeometry() {
    if (!m_notificationsItem)
        return;

    qreal maxHeight = 0;

    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedNotificationsGeometry(m_notificationsItem->geometry(), m_titleItem->geometry(), m_sliderItem->geometry(), m_calendarItem->paintGeometry(), m_dayTimeItem->paintGeometry(), &maxHeight);
    //m_notificationsItem->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_notificationsItem->setPos(geometry.topLeft());

    /* Whether actual size change should be deferred. */
    bool defer = false;

    /* Calculate slider target position. */
    QPointF sliderPos;
    if(!m_sliderVisible && m_notificationsVisible) {
        sliderPos = QPointF(m_sliderItem->pos().x(), m_controlsWidgetRect.bottom());
    } else if(m_sliderYAnimator->isRunning()) {
        sliderPos = QPointF(m_sliderItem->pos().x(), m_sliderYAnimator->targetValue().toReal());
        defer |= !qFuzzyEquals(sliderPos, m_sliderItem->pos()); /* If animation is running, then geometry sync should be deferred. */
    } else {
        sliderPos = m_sliderItem->pos();
    }

    /* Calculate calendar target position. */
    QPointF calendarPos;
    if(!m_calendarVisible && m_notificationsVisible) {
        calendarPos = QPointF(m_calendarItem->pos().x(), m_controlsWidgetRect.bottom());
    } else if(m_calendarSizeAnimator->isRunning()) {
        calendarPos = QPointF(m_calendarItem->pos().x(), sliderPos.y() - m_calendarSizeAnimator->targetValue().toSizeF().height());
        defer |= !qFuzzyEquals(calendarPos, m_calendarItem->pos()); /* If animation is running, then geometry sync should be deferred. */
    } else {
        calendarPos = m_calendarItem->pos();
    }

    /* Calculate title target position. */
    QPointF titlePos;
    if((!m_titleVisible || !m_titleUsed) && m_notificationsVisible) {
        titlePos = QPointF(m_titleItem->pos().x(), -m_titleItem->size().height());
    } else if(m_titleYAnimator->isRunning()) {
        titlePos = QPointF(m_titleItem->pos().x(), m_titleYAnimator->targetValue().toReal());
        defer |= !qFuzzyEquals(titlePos, m_titleItem->pos());
    } else {
        titlePos = m_titleItem->pos();
    }

    QPointF dayTimePos;
    if(!m_calendarVisible && m_notificationsVisible) {
        dayTimePos = QPointF(m_dayTimeItem->pos().x(), m_controlsWidgetRect.bottom());
    } else if(m_dayTimeSizeAnimator->isRunning()) {
        dayTimePos = QPointF(m_dayTimeItem->pos().x(), calendarPos.y() - m_dayTimeSizeAnimator->targetValue().toSizeF().height());
        defer |= !qFuzzyEquals(dayTimePos, m_dayTimeItem->pos()); /* If animation is running, then geometry sync should be deferred. */
    } else {
        dayTimePos = m_dayTimeItem->pos();
    }


    /* Calculate target geometry. */
    geometry = updatedNotificationsGeometry(m_notificationsItem->geometry(),
        QRectF(titlePos, m_titleItem->size()),
        QRectF(sliderPos, m_sliderItem->size()),
        QRectF(calendarPos, m_calendarItem->paintSize()),
        QRectF(dayTimePos, m_dayTimeItem->paintSize()),
        &maxHeight);
    if(qFuzzyEquals(geometry, m_notificationsItem->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if(defer && geometry.height() < m_notificationsItem->size().height())
        return;

    m_notificationsItem->resize(geometry.size());

    /* All tooltips should fit to rect of maxHeight */
    QRectF tooltipsEnclosingRect (
        m_controlsWidgetRect.left(),
        m_notificationsItem->y(),
        m_controlsWidgetRect.width(),
        maxHeight);
    m_notificationsItem->setToolTipsEnclosingRect(m_controlsWidget->mapRectToItem(m_notificationsItem, tooltipsEnclosingRect));
}

void QnWorkbenchUi::at_pinNotificationsAction_toggled(bool checked) {
    if (checked)
        setNotificationsOpened(true);
    updateViewportMargins();

    qnSettings->setNotificationsPinned(checked);
}

void QnWorkbenchUi::at_notificationsPinButton_toggled(bool checked) {
    m_notificationsPinned = checked;

    if(checked)
        setNotificationsOpened(true);

    updateViewportMargins();
}

void QnWorkbenchUi::at_notificationsShowingProcessor_hoverEntered() {
    if(!m_notificationsPinned && !isNotificationsOpened()) {
        setNotificationsOpened(true);

        /* So that the click that may follow won't hide it. */
        setNotificationsShowButtonUsed(false);
        QTimer::singleShot(300, this, SLOT(setNotificationsShowButtonUsed()));
    }

    m_notificationsHidingProcessor->forceHoverEnter();
    m_notificationsOpacityProcessor->forceHoverEnter();
}


void QnWorkbenchUi::at_notificationsItem_geometryChanged() {
    QRectF headerGeometry = m_controlsWidget->mapRectFromItem(m_notificationsItem, m_notificationsItem->headerGeometry());
    QRectF backgroundGeometry = m_controlsWidget->mapRectFromItem(m_notificationsItem, m_notificationsItem->visibleGeometry());

    /* Don't hide notifications item here. It will repaint itself when shown, which will
     * degrade performance. */

    m_notificationsBackgroundItem->setGeometry(backgroundGeometry);
    m_notificationsShowButton->setPos(QPointF(
        qMin(m_controlsWidgetRect.right(), headerGeometry.left()),
        (headerGeometry.top() + headerGeometry.bottom() - m_notificationsShowButton->size().height()) / 2
    ));
    m_notificationsPinButton->setPos(headerGeometry.topLeft() + QPointF(m_pinOffset, m_pinOffset));
    if (isNotificationsOpened())
        setNotificationsOpened(); //there is no check there but it will fix the X-coord animation

    updateViewportMargins();
}

void QnWorkbenchUi::createNotificationsWidget() {
    /* Notifications panel. */
    m_notificationsBackgroundItem = new QnControlBackgroundWidget(Qn::RightBorder, m_controlsWidget);

    m_notificationsItem = new QnNotificationsCollectionWidget(m_controlsWidget, 0, context());
    m_notificationsItem->setProperty(Qn::NoHandScrollOver, true);
    setHelpTopic(m_notificationsItem, Qn::MainWindow_Notifications_Help);

    m_notificationsPinButton = newPinButton(m_controlsWidget, action(Qn::PinNotificationsAction));
    m_notificationsPinButton->setFocusProxy(m_notificationsItem);

    QnBlinkingImageButtonWidget* blinker = new QnBlinkingImageButtonWidget(m_controlsWidget);
    m_notificationsShowButton = blinker;
    m_notificationsShowButton->setFixedSize(showHideButtonSize);
    m_notificationsShowButton->setImageMargins(showHideButtonMargins);
    m_notificationsShowButton->setCached(true);
    m_notificationsShowButton->setCheckable(true);
    m_notificationsShowButton->setIcon(qnSkin->icon("panel/slide_right.png", "panel/slide_left.png"));
    m_notificationsShowButton->setProperty(Qn::NoHandScrollOver, true);
    m_notificationsShowButton->setTransform(QTransform::fromScale(-1, 1));
    m_notificationsShowButton->setFocusProxy(m_notificationsItem);
    m_notificationsShowButton->stackBefore(m_notificationsItem);
    setHelpTopic(m_notificationsShowButton, Qn::MainWindow_Pin_Help);
    m_notificationsItem->setBlinker(blinker);

    m_notificationsOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_notificationsOpacityProcessor->addTargetItem(m_notificationsItem);
    m_notificationsOpacityProcessor->addTargetItem(m_notificationsShowButton);

    m_notificationsHidingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_notificationsHidingProcessor->addTargetItem(m_notificationsItem);
    m_notificationsHidingProcessor->addTargetItem(m_notificationsShowButton);
    m_notificationsHidingProcessor->setHoverLeaveDelay(closeConstrolsTimeoutMSec);
    m_notificationsHidingProcessor->setFocusLeaveDelay(closeConstrolsTimeoutMSec);

    m_notificationsShowingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_notificationsShowingProcessor->addTargetItem(m_notificationsShowButton);
    m_notificationsShowingProcessor->setHoverEnterDelay(250);

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

    connect(m_notificationsPinButton,           &QnImageButtonWidget::toggled,                          this,   &QnWorkbenchUi::at_notificationsPinButton_toggled);
    connect(m_notificationsShowButton,          &QnImageButtonWidget::toggled,                          this,   [this](bool checked){ if(!m_ignoreClickEvent) setNotificationsOpened(checked);});
    connect(m_notificationsOpacityProcessor,    &HoverFocusProcessor::hoverLeft,                        this,   &QnWorkbenchUi::updateNotificationsOpacityAnimated);
    connect(m_notificationsOpacityProcessor,    &HoverFocusProcessor::hoverEntered,                     this,   &QnWorkbenchUi::updateNotificationsOpacityAnimated);
    connect(m_notificationsOpacityProcessor,    &HoverFocusProcessor::hoverEntered,                     this,   &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_notificationsOpacityProcessor,    &HoverFocusProcessor::hoverLeft,                        this,   &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_notificationsHidingProcessor,     &HoverFocusProcessor::hoverFocusLeft,                   this,   [this](){ if(!m_notificationsPinned) setNotificationsOpened(false);});
    connect(m_notificationsShowingProcessor,    &HoverFocusProcessor::hoverEntered,                     this,   &QnWorkbenchUi::at_notificationsShowingProcessor_hoverEntered);
    connect(m_notificationsItem,                &QGraphicsWidget::geometryChanged,                      this,   &QnWorkbenchUi::at_notificationsItem_geometryChanged);
    connect(m_notificationsItem,                &QnNotificationsCollectionWidget::visibleSizeChanged,   this,   &QnWorkbenchUi::at_notificationsItem_geometryChanged);
    connect(m_notificationsItem,                &QnNotificationsCollectionWidget::sizeHintChanged,      this,   &QnWorkbenchUi::updateNotificationsGeometry);
}

#pragma endregion Notifications widget methods

#pragma region CalendarWidget

void QnWorkbenchUi::setCalendarVisible(bool visible, bool animate) {
    ensureAnimationAllowed(animate);

    bool changed = m_calendarVisible != visible;

    m_calendarVisible = visible;

    updateCalendarOpacity(animate);
    if(changed) {
        updateNotificationsGeometry();
    }
}

void QnWorkbenchUi::setCalendarOpacity(qreal opacity, bool animate) {
    ensureAnimationAllowed(animate);

    if(animate) {
        m_calendarOpacityAnimatorGroup->pause();
        opacityAnimator(m_calendarItem)->setTargetValue(opacity);
        opacityAnimator(m_dayTimeItem)->setTargetValue(opacity);
        opacityAnimator(m_calendarPinButton)->setTargetValue(opacity);
        opacityAnimator(m_dayTimeMinimizeButton)->setTargetValue(opacity);
        m_calendarOpacityAnimatorGroup->start();
    } else {
        m_calendarOpacityAnimatorGroup->stop();
        m_calendarItem->setOpacity(opacity);
        m_dayTimeItem->setOpacity(opacity);
        m_calendarPinButton->setOpacity(opacity);
        m_dayTimeMinimizeButton->setOpacity(opacity);
    }
}

void QnWorkbenchUi::setCalendarOpened(bool opened, bool animate) 
{
    if (!opened && isCalendarPinned() && action(Qn::ToggleCalendarAction)->isChecked())
    {
        return;
    }

    ensureAnimationAllowed(animate);

    m_inFreespace = false;

    m_calendarOpened = opened;

    QSizeF newSize = opened ? QSizeF(250, 200) : QSizeF(250, 0);
    if (animate) {
        m_calendarSizeAnimator->animateTo(newSize);
    } else {
        m_calendarSizeAnimator->stop();
        m_calendarItem->setPaintSize(newSize);
    }

    action(Qn::ToggleCalendarAction)->setChecked(opened);

    if(!opened)
        setDayTimeWidgetOpened(opened, animate);
}

void QnWorkbenchUi::setDayTimeWidgetOpened(bool opened, bool animate) {
    ensureAnimationAllowed(animate);

    m_inFreespace = false;

    m_dayTimeOpened = opened;

    QSizeF newSize = opened ? QSizeF(250, 120) : QSizeF(250, 0);
    if (animate) {
        m_dayTimeSizeAnimator->animateTo(newSize);
    } else {
        m_dayTimeSizeAnimator->stop();
        m_dayTimeItem->setPaintSize(newSize);
    }
}

void QnWorkbenchUi::updateCalendarOpacity(bool animate) {
    if (qnSettings->lightMode() & Qn::LightModeNoOpacity) {
        qreal opacity = m_calendarVisible ? opaque : hidden;
        setCalendarOpacity(opacity, false);
        return;
    }

    if(!m_calendarVisible) {
        setCalendarOpacity(0.0, animate);
    } else {
        if(m_calendarOpacityProcessor->isHovered()) {
            setCalendarOpacity(hoverCalendarOpacity, animate);
        } else {
            setCalendarOpacity(normalCalendarOpacity, animate);
        }
    }
}

void QnWorkbenchUi::updateCalendarVisibility(bool animate) {
    ensureAnimationAllowed(animate);

    bool calendarEmpty = true;
    if (QnCalendarWidget* c = dynamic_cast<QnCalendarWidget *>(m_calendarItem->widget()))
        calendarEmpty = c->isEmpty(); /* Small hack. We have a signal that updates visibility if a calendar receive new data */

    bool calendarEnabled = !calendarEmpty && (navigator()->currentWidget() && navigator()->currentWidget()->resource()->flags() & Qn::utc);
    action(Qn::ToggleCalendarAction)->setEnabled(calendarEnabled); // TODO: #GDM #Common does this belong here?

    bool calendarVisible = calendarEnabled && m_sliderVisible && isSliderOpened();

    if(m_inactive)
        setCalendarVisible(calendarVisible && isHovered(), animate);
    else
        setCalendarVisible(calendarVisible, animate);

    if(!calendarVisible)
        setCalendarOpened(false);
}

QRectF QnWorkbenchUi::updatedCalendarGeometry(const QRectF &sliderGeometry) {
    QRectF geometry = m_calendarItem->paintGeometry();
    geometry.moveRight(m_controlsWidgetRect.right());
    geometry.moveBottom(sliderGeometry.top());
    return geometry;
}

void QnWorkbenchUi::updateCalendarGeometry() {
    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedCalendarGeometry(m_sliderItem->geometry());
    m_calendarItem->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_calendarItem->setPos(geometry.topLeft());
}

QRectF QnWorkbenchUi::updatedDayTimeWidgetGeometry(const QRectF &sliderGeometry, const QRectF &calendarGeometry) {
    QRectF geometry = m_dayTimeItem->paintGeometry();
    geometry.moveRight(sliderGeometry.right());
    geometry.moveBottom(calendarGeometry.top());
    return geometry;
}

void QnWorkbenchUi::updateDayTimeWidgetGeometry() {
    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedDayTimeWidgetGeometry(m_sliderItem->geometry(), m_calendarItem->geometry());
    m_dayTimeItem->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_dayTimeItem->setPos(geometry.topLeft());
}

void QnWorkbenchUi::at_calendarItem_paintGeometryChanged() 
{
    const QRectF paintGeometry = m_calendarItem->paintGeometry();
    m_calendarPinButton->setPos(paintGeometry.topRight() + m_calendarPinOffset);

    if(m_inCalendarGeometryUpdate)
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

    if(m_inDayTimeGeometryUpdate)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_inDayTimeGeometryUpdate, true);

    updateDayTimeWidgetGeometry();
    updateNotificationsGeometry();
}

void QnWorkbenchUi::at_calendarWidget_dateClicked(const QDate &date) 
{
    const bool sameDate = (m_dayTimeWidget->date() == date); 
    m_dayTimeWidget->setDate(date);

    if(isCalendarOpened())
    {
        const bool shouldBeOpened = !sameDate || !m_dayTimeOpened;
        setDayTimeWidgetOpened(shouldBeOpened, true);
    }
}

void QnWorkbenchUi::createCalendarWidget() {
    QnCalendarWidget *calendarWidget = new QnCalendarWidget();    
    setHelpTopic(calendarWidget, Qn::MainWindow_Calendar_Help);
    navigator()->setCalendar(calendarWidget);

    m_dayTimeWidget = new QnDayTimeWidget();
    setHelpTopic(m_dayTimeWidget, Qn::MainWindow_DayTimePicker_Help);
    navigator()->setDayTimeWidget(m_dayTimeWidget);

    m_calendarItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_calendarItem->setWidget(calendarWidget);
    calendarWidget->installEventFilter(m_calendarItem);
    m_calendarItem->resize(250, 200);
    m_calendarItem->setProperty(Qn::NoHandScrollOver, true);

    m_calendarPinButton = newPinButton(m_controlsWidget, action(Qn::PinCalendarAction));
    m_calendarPinButton->setFocusProxy(m_calendarItem);

    m_dayTimeItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_dayTimeItem->setWidget(m_dayTimeWidget);
    m_dayTimeWidget->installEventFilter(m_calendarItem);
    m_dayTimeItem->resize(250, 120);
    m_dayTimeItem->setProperty(Qn::NoHandScrollOver, true);
    m_dayTimeItem->stackBefore(m_calendarItem);

    m_dayTimeMinimizeButton = newActionButton(action(Qn::MinimizeDayTimeViewAction)
        , kDefaultSizeMultiplier, kDefaultHelpTopicId, m_controlsWidget);
    m_dayTimeMinimizeButton->setFocusProxy(m_dayTimeItem);

    m_calendarOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_calendarOpacityProcessor->addTargetItem(m_calendarItem);
    m_calendarOpacityProcessor->addTargetItem(m_dayTimeItem);
    m_calendarOpacityProcessor->addTargetItem(m_calendarPinButton);
    m_calendarOpacityProcessor->addTargetItem(m_dayTimeMinimizeButton);

    HoverFocusProcessor* calendarHidingProcessor = new HoverFocusProcessor(m_controlsWidget);
    calendarHidingProcessor->addTargetItem(m_calendarItem);
    calendarHidingProcessor->addTargetItem(m_dayTimeItem);
    calendarHidingProcessor->addTargetItem(m_calendarPinButton);
    calendarHidingProcessor->setHoverLeaveDelay(closeConstrolsTimeoutMSec);
    calendarHidingProcessor->setFocusLeaveDelay(closeConstrolsTimeoutMSec);

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

    connect(calendarWidget,             &QnCalendarWidget::emptyChanged,        this,   &QnWorkbenchUi::updateCalendarVisibilityAnimated);
    connect(calendarWidget,             &QnCalendarWidget::dateClicked,         this,   &QnWorkbenchUi::at_calendarWidget_dateClicked);
    connect(m_dayTimeItem,              &QnMaskedProxyWidget::paintRectChanged, this,   &QnWorkbenchUi::at_dayTimeItem_paintGeometryChanged);
    connect(m_dayTimeItem,              &QGraphicsWidget::geometryChanged,      this,   &QnWorkbenchUi::at_dayTimeItem_paintGeometryChanged);
    connect(m_calendarOpacityProcessor, &HoverFocusProcessor::hoverLeft,        this,   &QnWorkbenchUi::updateCalendarOpacityAnimated);
    connect(m_calendarOpacityProcessor, &HoverFocusProcessor::hoverEntered,     this,   &QnWorkbenchUi::updateCalendarOpacityAnimated);
    connect(m_calendarOpacityProcessor, &HoverFocusProcessor::hoverEntered,     this,   &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_calendarOpacityProcessor, &HoverFocusProcessor::hoverLeft,        this,   &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(calendarHidingProcessor,    &HoverFocusProcessor::hoverLeft,        this,   [this](){ setCalendarOpened(false);});
    connect(m_calendarItem,             &QnMaskedProxyWidget::paintRectChanged, this,   &QnWorkbenchUi::at_calendarItem_paintGeometryChanged);
    connect(m_calendarItem,             &QGraphicsWidget::geometryChanged,      this,   &QnWorkbenchUi::at_calendarItem_paintGeometryChanged);
    connect(action(Qn::ToggleCalendarAction), &QAction::toggled,                this,   [this](bool checked) { setCalendarOpened(checked); });
    connect(action(Qn::MinimizeDayTimeViewAction), &QAction::triggered,        this,   [this]() { setDayTimeWidgetOpened(false, true); });

    enum { kCellsCountOffset = 2 };
    const int size = calendarWidget->headerHeight();
    m_calendarPinOffset = QPoint(-kCellsCountOffset * size
        , (size - m_calendarPinButton->size().height()) / 2.0f);
    m_dayTimeOffset = QPoint(-m_dayTimeWidget->headerHeight() , 0);
}

#pragma endregion Calendar and DayTime widget methods

#pragma region SliderWidget

bool QnWorkbenchUi::isThumbnailsVisible() const {
    return !qFuzzyCompare(m_sliderItem->geometry().height(), m_sliderItem->effectiveSizeHint(Qt::MinimumSize).height());
}

void QnWorkbenchUi::setThumbnailsVisible(bool visible) {
    if(visible == isThumbnailsVisible())
        return;

    qreal sliderHeight = m_sliderItem->effectiveSizeHint(Qt::MinimumSize).height();
    if(!visible) {
        m_lastThumbnailsHeight = m_sliderItem->geometry().height() - sliderHeight;
    } else {
        sliderHeight += m_lastThumbnailsHeight;
    }

    QRectF geometry = m_sliderItem->geometry();
    geometry.setHeight(sliderHeight);
    m_sliderItem->setGeometry(geometry);
}

bool QnWorkbenchUi::isSliderOpened() const {
    return action(Qn::ToggleSliderAction)->isChecked();
}

void QnWorkbenchUi::setSliderOpened(bool opened, bool animate, bool save) {
    if (qnSettings->isVideoWallMode()) {
        opened = true;
        save = false;
    }

    ensureAnimationAllowed(animate);

    m_inFreespace = false;

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(Qn::ToggleSliderAction)->setChecked(opened);

    qreal newY = m_controlsWidgetRect.bottom() + (opened ? -m_sliderItem->size().height() : 48.0 /* So that tooltips are not opened. */);
    if (animate) {
        m_sliderYAnimator->animateTo(newY);
    } else {
        m_sliderYAnimator->stop();
        m_sliderItem->setY(newY);
    }

    updateCalendarVisibility(animate);

    if (save)
        qnSettings->setSliderOpened(opened);

    static_cast<QnResizerWidget*>(m_sliderResizerWidget)->setEnabled(opened);
}

void QnWorkbenchUi::setSliderVisible(bool visible, bool animate) {
    ensureAnimationAllowed(animate);

    bool changed = m_sliderVisible != visible;

    m_sliderVisible = visible;
    if (qnSettings->isVideoWallMode()) {
        if (visible)
            m_sliderAutoHideTimer->start(sliderAutoHideTimeoutMSec);
        else
            m_sliderAutoHideTimer->stop();
    }

    updateSliderOpacity(animate);
    if(changed) {
        updateTreeGeometry();
        updateNotificationsGeometry();
        updateCalendarVisibility(animate);
        m_sliderItem->setEnabled(m_sliderVisible); /* So that it doesn't handle mouse events while disappearing. */
    }
}

void QnWorkbenchUi::setSliderOpacity(qreal opacity, bool animate) {
    ensureAnimationAllowed(animate);

    if(animate) {
        m_sliderOpacityAnimatorGroup->pause();
        opacityAnimator(m_sliderItem)->setTargetValue(opacity);
        opacityAnimator(m_sliderShowButton)->setTargetValue(opacity);
        m_sliderOpacityAnimatorGroup->start();
    } else {
        m_sliderOpacityAnimatorGroup->stop();
        m_sliderItem->setOpacity(opacity);
        m_sliderShowButton->setOpacity(opacity);
    }

    m_sliderResizerWidget->setVisible(!qFuzzyIsNull(opacity));
}

void QnWorkbenchUi::setSliderZoomButtonsOpacity(qreal opacity, bool animate) {
    ensureAnimationAllowed(animate);

    if(animate) {
        opacityAnimator(m_sliderZoomButtonsWidget)->animateTo(opacity);
    } else {
        m_sliderZoomButtonsWidget->setOpacity(opacity);
    }
}

void QnWorkbenchUi::updateSliderOpacity(bool animate) {
    if (qnSettings->lightMode() & Qn::LightModeNoOpacity) {
        qreal opacity = m_sliderVisible ? opaque : hidden;
        setSliderOpacity(opacity, false);
        setSliderZoomButtonsOpacity(opacity, false);
        return;
    }

    if(!m_sliderVisible) {
        setSliderOpacity(0.0, animate);
        setSliderZoomButtonsOpacity(0.0, animate);
    } else {
        if(m_sliderOpacityProcessor->isHovered()) {
            setSliderOpacity(hoverSliderOpacity, animate);
            setSliderZoomButtonsOpacity(hoverSliderOpacity, animate);
        } else {
            setSliderOpacity(normalSliderOpacity, animate);
            setSliderZoomButtonsOpacity(0.0, animate);
        }
    }
}

void QnWorkbenchUi::updateSliderResizerGeometry() {
    if(m_ignoreSliderResizerGeometryLater) {
        QTimer::singleShot(1, this, SLOT(updateSliderResizerGeometry()));
        return;
    }

    QnTimeSlider *timeSlider = m_sliderItem->timeSlider();
    QRectF timeSliderRect = timeSlider->rect();

    QRectF sliderResizerGeometry = QRectF(
        m_controlsWidget->mapFromItem(timeSlider, timeSliderRect.topLeft()),
        m_controlsWidget->mapFromItem(timeSlider, timeSliderRect.topRight())
        );
    sliderResizerGeometry.moveTo(sliderResizerGeometry.topLeft() - QPointF(0, 8));
    sliderResizerGeometry.setHeight(16);

    if(!qFuzzyEquals(sliderResizerGeometry, m_sliderResizerWidget->geometry())) {
        QN_SCOPED_VALUE_ROLLBACK(&m_ignoreSliderResizerGeometryLater, true);

        m_sliderResizerWidget->setGeometry(sliderResizerGeometry);

        /* This one is needed here as we're in a handler and thus geometry change doesn't adjust position =(. */
        m_sliderResizerWidget->setPos(sliderResizerGeometry.topLeft());  // TODO: #Elric remove this ugly hack.
    }
}

void QnWorkbenchUi::updateSliderZoomButtonsGeometry() {
    QPointF pos = m_sliderItem->timeSlider()->mapToItem(m_controlsWidget, m_sliderItem->timeSlider()->rect().topLeft());

    m_sliderZoomButtonsWidget->setPos(pos);
}

void QnWorkbenchUi::at_sliderResizerWidget_wheelEvent(QObject *, QEvent *event) {
    QGraphicsSceneWheelEvent *oldEvent = static_cast<QGraphicsSceneWheelEvent *>(event);

    QGraphicsSceneWheelEvent newEvent(QEvent::GraphicsSceneWheel);
    newEvent.setDelta(oldEvent->delta());
    newEvent.setPos(m_sliderItem->timeSlider()->mapFromItem(m_sliderResizerWidget, oldEvent->pos()));
    newEvent.setScenePos(oldEvent->scenePos());
    display()->scene()->sendEvent(m_sliderItem->timeSlider(), &newEvent);
}

void QnWorkbenchUi::at_sliderItem_geometryChanged() {
    setSliderOpened(isSliderOpened(), m_sliderYAnimator->isRunning(), false); /* Re-adjust to screen sides. */

    updateTreeGeometry();
    updateNotificationsGeometry();

    updateViewportMargins();
    updateSliderResizerGeometry();
    updateCalendarGeometry();
    updateSliderZoomButtonsGeometry();
    updateDayTimeWidgetGeometry();
    updateSearchGeometry();

    QRectF geometry = m_sliderItem->geometry();
    m_sliderShowButton->setPos(QPointF(
        (geometry.left() + geometry.right() - m_titleShowButton->size().height()) / 2,
        qMin(m_controlsWidgetRect.bottom(), geometry.top())
        ));
}

void QnWorkbenchUi::at_sliderResizerWidget_geometryChanged() {
    if(m_ignoreSliderResizerGeometryChanges)
        return;

    QRectF sliderResizerGeometry = m_sliderResizerWidget->geometry();
    if (!sliderResizerGeometry.isValid()) {
        updateSliderResizerGeometry();
        return;
    }

    QRectF sliderGeometry = m_sliderItem->geometry();

    qreal targetHeight = sliderGeometry.bottom() - sliderResizerGeometry.center().y();
    qreal minHeight = m_sliderItem->effectiveSizeHint(Qt::MinimumSize).height();
    qreal jmpHeight = minHeight + 48.0;
    qreal maxHeight = minHeight + 196.0;

    if(targetHeight < (minHeight + jmpHeight) / 2) {
        targetHeight = minHeight;
    } else if(targetHeight < jmpHeight) {
        targetHeight = jmpHeight;
    } else if(targetHeight > maxHeight) {
        targetHeight = maxHeight;
    }

    if(!qFuzzyCompare(sliderGeometry.height(), targetHeight)) {
        qreal sliderTop = sliderGeometry.top();
        sliderGeometry.setHeight(targetHeight);
        sliderGeometry.moveTop(sliderTop);

        QN_SCOPED_VALUE_ROLLBACK(&m_ignoreSliderResizerGeometryChanges, true);
        m_sliderItem->setGeometry(sliderGeometry);
    }

    updateSliderResizerGeometry();

    action(Qn::ToggleThumbnailsAction)->setChecked(isThumbnailsVisible());
}

void QnWorkbenchUi::createSliderWidget() 
{
    m_sliderResizerWidget = new QnResizerWidget(Qt::Vertical, m_controlsWidget);
    m_sliderResizerWidget->setProperty(Qn::NoHandScrollOver, true);

    m_sliderItem = new QnNavigationItem(m_controlsWidget);
    m_sliderItem->setFrameColor(QColor(110, 110, 110, 128));
    m_sliderItem->setFrameWidth(1.0);
    m_sliderItem->timeSlider()->toolTipItem()->setParentItem(m_controlsWidget);

    m_sliderItem->setProperty(Qn::NoHandScrollOver, true);
    m_sliderItem->timeSlider()->toolTipItem()->setProperty(Qn::NoHandScrollOver, true);
    m_sliderItem->speedSlider()->toolTipItem()->setProperty(Qn::NoHandScrollOver, true);
    m_sliderItem->volumeSlider()->toolTipItem()->setProperty(Qn::NoHandScrollOver, true);

    m_sliderShowButton = newShowHideButton(m_controlsWidget, action(Qn::ToggleSliderAction));
    {
        QTransform transform;
        transform.rotate(-90);
        m_sliderShowButton->setTransform(transform);
    }
    m_sliderShowButton->setFocusProxy(m_sliderItem);

    if (qnSettings->isVideoWallMode()) {
        m_sliderShowButton->setVisible(false);

        m_sliderAutoHideTimer = new QTimer(this);
        connect(m_sliderAutoHideTimer, &QTimer::timeout, this, [this](){setSliderVisible(false, true);});
    }

    QnImageButtonWidget *sliderZoomOutButton = new QnImageButtonWidget();
    sliderZoomOutButton->setIcon(qnSkin->icon("slider/buttons/zoom_out.png"));
    sliderZoomOutButton->setPreferredSize(16, 16);

    QnImageButtonWidget *sliderZoomInButton = new QnImageButtonWidget();
    sliderZoomInButton->setIcon(qnSkin->icon("slider/buttons/zoom_in.png"));
    sliderZoomInButton->setPreferredSize(16, 16);

    QGraphicsLinearLayout *sliderZoomButtonsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    sliderZoomButtonsLayout->setSpacing(0.0);
    sliderZoomButtonsLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    sliderZoomButtonsLayout->addItem(sliderZoomOutButton);
    sliderZoomButtonsLayout->addItem(sliderZoomInButton);

    m_sliderZoomButtonsWidget = new GraphicsWidget(m_controlsWidget);
    m_sliderZoomButtonsWidget->setLayout(sliderZoomButtonsLayout);
    m_sliderZoomButtonsWidget->setOpacity(0.0);

    /* There is no stackAfter function, so we have to resort to ugly copypasta. */
    m_sliderShowButton->stackBefore(m_sliderItem->timeSlider()->toolTipItem());
    m_sliderResizerWidget->stackBefore(m_sliderShowButton);
    m_sliderResizerWidget->stackBefore(m_sliderZoomButtonsWidget);
    m_sliderItem->stackBefore(m_sliderResizerWidget);

    m_sliderOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_sliderOpacityProcessor->addTargetItem(m_sliderItem);
    m_sliderOpacityProcessor->addTargetItem(m_sliderItem->timeSlider()->toolTipItem());
    m_sliderOpacityProcessor->addTargetItem(m_sliderShowButton);
    m_sliderOpacityProcessor->addTargetItem(m_sliderResizerWidget);
    m_sliderOpacityProcessor->addTargetItem(m_sliderZoomButtonsWidget);

    m_sliderYAnimator = new VariantAnimator(this);
    m_sliderYAnimator->setTimer(m_instrumentManager->animationTimer());
    m_sliderYAnimator->setTargetObject(m_sliderItem);
    m_sliderYAnimator->setAccessor(new PropertyAccessor("y"));
    //m_sliderYAnimator->setSpeed(m_sliderItem->size().height() * 2.0); // TODO: #Elric why height is zero at this point?
    m_sliderYAnimator->setSpeed(70.0 * 2.0);
    m_sliderYAnimator->setTimeLimit(500);

    m_sliderOpacityAnimatorGroup = new AnimatorGroup(this);
    m_sliderOpacityAnimatorGroup->setTimer(m_instrumentManager->animationTimer());
    m_sliderOpacityAnimatorGroup->addAnimator(opacityAnimator(m_sliderItem));
    m_sliderOpacityAnimatorGroup->addAnimator(opacityAnimator(m_sliderShowButton)); /* Speed of 1.0 is OK here. */

    QnSingleEventEater *sliderWheelEater = new QnSingleEventEater(this);
    sliderWheelEater->setEventType(QEvent::GraphicsSceneWheel);
    m_sliderResizerWidget->installEventFilter(sliderWheelEater);

    QnSingleEventSignalizer *sliderWheelSignalizer = new QnSingleEventSignalizer(this);
    sliderWheelSignalizer->setEventType(QEvent::GraphicsSceneWheel);
    m_sliderResizerWidget->installEventFilter(sliderWheelSignalizer);

    connect(sliderZoomInButton,         &QnImageButtonWidget::pressed,              this,           [this](){ m_sliderZoomingIn = true;});
    connect(sliderZoomInButton,         &QnImageButtonWidget::released,             this,           [this](){ m_sliderZoomingIn = false; m_sliderItem->timeSlider()->hurryKineticAnimations();});
    connect(sliderZoomOutButton,        &QnImageButtonWidget::pressed,              this,           [this](){ m_sliderZoomingOut = true;});
    connect(sliderZoomOutButton,        &QnImageButtonWidget::released,             this,           [this](){ m_sliderZoomingOut = false; m_sliderItem->timeSlider()->hurryKineticAnimations();});

    connect(sliderWheelSignalizer,      &QnAbstractEventSignalizer::activated,      this,           &QnWorkbenchUi::at_sliderResizerWidget_wheelEvent);
    connect(m_sliderOpacityProcessor,   &HoverFocusProcessor::hoverEntered,         this,           &QnWorkbenchUi::updateSliderOpacityAnimated);
    connect(m_sliderOpacityProcessor,   &HoverFocusProcessor::hoverLeft,            this,           &QnWorkbenchUi::updateSliderOpacityAnimated);
    connect(m_sliderOpacityProcessor,   &HoverFocusProcessor::hoverEntered,         this,           &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_sliderOpacityProcessor,   &HoverFocusProcessor::hoverLeft,            this,           &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_sliderItem,               &QGraphicsWidget::geometryChanged,          this,           &QnWorkbenchUi::at_sliderItem_geometryChanged);
    connect(m_sliderResizerWidget,      &QGraphicsWidget::geometryChanged,          this,           &QnWorkbenchUi::at_sliderResizerWidget_geometryChanged);
    connect(navigator(),                &QnWorkbenchNavigator::currentWidgetChanged,this,           &QnWorkbenchUi::updateControlsVisibilityAnimated);
    if (qnSettings->isVideoWallMode()) {
        connect(navigator(),           &QnWorkbenchNavigator::positionChanged,      this,           &QnWorkbenchUi::updateCalendarVisibilityAnimated);
        connect(navigator(),           &QnWorkbenchNavigator::speedChanged,         this,           &QnWorkbenchUi::updateCalendarVisibilityAnimated);
    }
    connect(action(Qn::ToggleTourModeAction),   &QAction::toggled,                  this,           &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(action(Qn::ToggleThumbnailsAction), &QAction::toggled,                  this,           [this](bool checked){ setThumbnailsVisible(checked); });
    connect(action(Qn::ToggleSliderAction),     &QAction::toggled,                  this,           [this](bool checked){ if (!m_ignoreClickEvent) setSliderOpened(checked);});
}

#pragma endregion Slider methods

#pragma region DebugWidget

void QnWorkbenchUi::createDebugWidget() {
    m_debugOverlayLabel = new QnDebugProxyLabel(m_controlsWidget);
    m_debugOverlayLabel->setAcceptedMouseButtons(0);
    m_debugOverlayLabel->setAcceptHoverEvents(false);
    m_debugOverlayLabel->setMessagesLimit(40);
    setPaletteColor(m_debugOverlayLabel, QPalette::Window, QColor(127, 127, 127, 60));
    setPaletteColor(m_debugOverlayLabel, QPalette::WindowText,  QColor(63, 255, 216));
    auto updateDebugGeometry = [&]() {
        QPointF pos = QPointF(m_titleItem->geometry().bottomLeft());
        if(qFuzzyEquals(pos, m_debugOverlayLabel->pos()))
            return;
        m_debugOverlayLabel->setPos(pos);
    };
    m_debugOverlayLabel->setVisible(false);
    connect(m_titleItem, &QGraphicsWidget::geometryChanged, this, updateDebugGeometry);
    debugLabel = m_debugOverlayLabel;
    previousMsgHandler = qInstallMessageHandler(uiMsgHandler);

    display()->view()->addAction(action(Qn::ShowDebugOverlayAction));
    connect(action(Qn::ShowDebugOverlayAction),  &QAction::toggled,  this, [&](bool toggled){m_debugOverlayLabel->setVisible(toggled);});
}

#pragma endregion Debug overlay methods

#pragma region SearchWidget

QRectF QnWorkbenchUi::updatedSearchGeometry(const QRectF &sliderGeometry) {
    QRectF geometry = m_searchWidget->paintGeometry();
    geometry.moveRight(m_controlsWidgetRect.right());
    geometry.moveBottom(sliderGeometry.top());
    return geometry;
}

void QnWorkbenchUi::updateSearchGeometry() {
    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedSearchGeometry(m_sliderItem->geometry());
    m_searchWidget->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_searchWidget->setPos(geometry.topLeft());
}

void QnWorkbenchUi::at_searchItem_paintGeometryChanged() {
    if(m_inSearchGeometryUpdate)
        return;
    QN_SCOPED_VALUE_ROLLBACK(&m_inSearchGeometryUpdate, true);
    updateSearchGeometry();
}

void QnWorkbenchUi::setSearchVisible(bool visible, bool animate) {
    ensureAnimationAllowed(animate);

    bool changed = m_searchVisible != visible;

    m_searchVisible = visible;

    updateSearchOpacity(animate);
    if(changed)
        updateSearchGeometry();
}

void QnWorkbenchUi::setSearchOpened(bool opened, bool animate) {
    ensureAnimationAllowed(animate);

    m_inFreespace = false;

    const bool wasOpened = m_searchOpened;
    m_searchOpened = opened;

    QSizeF newSize = opened ? QSizeF(250, 21) : QSizeF(250, 0);
    if (animate) {
        m_searchSizeAnimator->animateTo(newSize);
    } else {
        m_searchSizeAnimator->stop();
        m_searchWidget->setPaintSize(newSize);
    }
    
    QnSearchLineEdit *searchWidget = navigator()->bookmarksSearchWidget();
    if (opened && !wasOpened)
    {
        searchWidget->enterKeyPressed();
    }
    
    searchWidget->setEnabled(opened);
    action(Qn::ToggleBookmarksSearchAction)->setChecked(opened);
}

void QnWorkbenchUi::updateSearchOpacity(bool animate) {
    if (qnSettings->lightMode() & Qn::LightModeNoOpacity) {
        qreal opacity = m_searchVisible ? opaque : hidden;
        setSearchOpacity(opacity, false);
        return;
    }

    if(!m_searchVisible) {
        setSearchOpacity(0.0, animate);
    } else {
        if(m_searchOpacityProcessor->isHovered()) {
            setSearchOpacity(hoverSearchOpacity, animate);
        } else {
            setSearchOpacity(normalSearchOpacity, animate);
        }
    }
}

void QnWorkbenchUi::setSearchOpacity(qreal opacity, bool animate) {
    ensureAnimationAllowed(animate);

    if(animate) {
        m_searchOpacityAnimatorGroup->pause();
        opacityAnimator(m_searchWidget)->setTargetValue(opacity);
        m_searchOpacityAnimatorGroup->start();
    } else {
        m_searchOpacityAnimatorGroup->stop();
        m_searchWidget->setOpacity(opacity);
    }
}

void QnWorkbenchUi::updateSearchVisibility(bool animate) {
    ensureAnimationAllowed(animate);

    bool searchVisible = action(Qn::ToggleBookmarksSearchAction)->isEnabled() && m_sliderVisible && isSliderOpened();

    if(m_inactive)
        setSearchVisible(searchVisible && isHovered(), animate);
    else
        setSearchVisible(searchVisible, animate);

    if(!searchVisible)
        setSearchOpened(false);
}

void QnWorkbenchUi::createSearchWidget() {
    QnSearchLineEdit *searchLine = new QnSearchLineEdit();
    searchLine->setAttribute(Qt::WA_TranslucentBackground);
    searchLine->resize(250, 21);
    connect(searchLine, &QnSearchLineEdit::escKeyPressed, this, [this]() { setSearchOpened(false, true); } );

    navigator()->setBookmarksSearchWidget(searchLine);

    m_searchWidget = new QnMaskedProxyWidget(m_controlsWidget);
    m_searchWidget->setWidget(searchLine);
    m_searchWidget->setProperty(Qn::NoHandScrollOver, true);
    connect(m_searchWidget,   &QnMaskedProxyWidget::paintRectChanged,     this,   &QnWorkbenchUi::at_searchItem_paintGeometryChanged);
    connect(m_searchWidget,   &QGraphicsWidget::geometryChanged,          this,   &QnWorkbenchUi::at_searchItem_paintGeometryChanged);

    // slider should be highlighted when search widget is hovered
    m_sliderOpacityProcessor->addTargetItem(m_searchWidget);

    m_searchOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_searchOpacityProcessor->addTargetItem(m_searchWidget);
    connect(m_searchOpacityProcessor, &HoverFocusProcessor::hoverLeft,        this,   &QnWorkbenchUi::updateSearchOpacityAnimated);
    connect(m_searchOpacityProcessor, &HoverFocusProcessor::hoverEntered,     this,   &QnWorkbenchUi::updateSearchOpacityAnimated);
    connect(m_searchOpacityProcessor, &HoverFocusProcessor::hoverEntered,     this,   &QnWorkbenchUi::updateControlsVisibilityAnimated);
    connect(m_searchOpacityProcessor, &HoverFocusProcessor::hoverLeft,        this,   &QnWorkbenchUi::updateControlsVisibilityAnimated);

    m_searchOpacityAnimatorGroup = new AnimatorGroup(this);
    m_searchOpacityAnimatorGroup->setTimer(m_instrumentManager->animationTimer());
    m_searchOpacityAnimatorGroup->addAnimator(opacityAnimator(m_searchWidget));

    m_searchSizeAnimator = new VariantAnimator(this);
    m_searchSizeAnimator->setTimer(m_instrumentManager->animationTimer());
    m_searchSizeAnimator->setTargetObject(m_searchWidget);
    m_searchSizeAnimator->setAccessor(new PropertyAccessor("paintSize"));
    m_searchSizeAnimator->setSpeed(100.0 * 2.0);
    m_searchSizeAnimator->setTimeLimit(500);

    connect(action(Qn::ToggleBookmarksSearchAction), &QAction::toggled,  this,   [this, searchLine](bool checked) {
        setSearchOpened(checked); 
        searchLine->setFocus();
    });
}


#pragma endregion Search widget methods

#pragma region FpsWidget

bool QnWorkbenchUi::isFpsVisible() const {
    return m_fpsItem->isVisible();
}

void QnWorkbenchUi::setFpsVisible(bool fpsVisible) {
    if(fpsVisible == isFpsVisible())
        return;

    m_fpsItem->setVisible(fpsVisible);
    m_fpsCountingInstrument->setEnabled(fpsVisible);

    if(fpsVisible)
        m_fpsCountingInstrument->recursiveEnable();
    else
        m_fpsCountingInstrument->recursiveDisable();

    m_fpsItem->setText(QString());

    action(Qn::ShowFpsAction)->setChecked(fpsVisible);
}

void QnWorkbenchUi::updateFpsGeometry() {
    QPointF pos = QPointF(
        m_controlsWidgetRect.right() - m_fpsItem->size().width(),
        m_titleItem->geometry().bottom()
        );

    if(qFuzzyEquals(pos, m_fpsItem->pos()))
        return;

    m_fpsItem->setPos(pos);
}

void QnWorkbenchUi::createFpsWidget() {
    m_fpsItem = new QnProxyLabel(m_controlsWidget);
    m_fpsItem->setAcceptedMouseButtons(0);
    m_fpsItem->setAcceptHoverEvents(false);
    setPaletteColor(m_fpsItem, QPalette::Window, Qt::transparent);
    setPaletteColor(m_fpsItem, QPalette::WindowText,  QColor(63, 159, 216));

    display()->view()->addAction(action(Qn::ShowFpsAction));
    connect(action(Qn::ShowFpsAction),  &QAction::toggled,                      this,   &QnWorkbenchUi::setFpsVisible);
    connect(m_fpsItem,                  &QGraphicsWidget::geometryChanged,      this,   &QnWorkbenchUi::updateFpsGeometry);
    setFpsVisible(false);
}

#pragma endregion Fps widget methods
