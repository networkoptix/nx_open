#include "workbench_ui.h"
#include <cassert>
#include <cmath> /* For std::floor. */

#include <QtCore/QSettings>
#include <QtCore/QTimer>

#include <QtGui/QComboBox>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsProxyWidget>
#include <QtGui/QGraphicsLinearLayout>
#include <QtGui/QStyle>
#include <QtGui/QApplication>
#include <QtGui/QMenu>

#include <core/dataprovider/abstract_streamdataprovider.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/layout_resource.h>

#include <camera/resource_display.h>
#include <camera/video_camera.h>

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
#include <ui/graphics/items/generic/simple_frame_widget.h>
#include <ui/graphics/items/generic/tool_tip_widget.h>
#include <ui/graphics/items/generic/ui_elements_widget.h>
#include <ui/graphics/items/generic/proxy_label.h>
#include <ui/graphics/items/controls/navigation_item.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/controls/speed_slider.h>
#include <ui/graphics/items/controls/volume_slider.h>
#include <ui/graphics/items/controls/time_scroll_bar.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/standard/graphics_message_box.h>
#include <ui/graphics/items/notifications/notifications_collection_widget.h>
#include <ui/common/palette.h>
#include <ui/processors/hover_processor.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/calendar_widget.h>
#include <ui/widgets/resource_browser_widget.h>
#include <ui/widgets/layout_tab_bar.h>
#include <ui/style/skin.h>
#include <ui/style/noptix_style.h>
#include <ui/workaround/system_menu_event.h>
#include <ui/screen_recording/screen_recorder.h>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/checked_cast.h>
#include <client/client_settings.h>

#include "openal/qtvaudiodevice.h"
#include "core/resource_managment/resource_pool.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"

#include "watchers/workbench_render_watcher.h"
#include "watchers/workbench_motion_display_watcher.h"
#include "workbench.h"
#include "workbench_display.h"
#include "workbench_layout.h"
#include "workbench_context.h"
#include "workbench_navigator.h"
#include "workbench_access_controller.h"


namespace {

    QnImageButtonWidget *newActionButton(QAction *action, qreal sizeMultiplier = 1.0, int helpTopicId = -1, QGraphicsItem *parent = NULL) {
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
        button->resize(15, 45);
        if (action)
            button->setDefaultAction(action);
        else
            button->setCheckable(true);
        button->setIcon(qnSkin->icon("panel/slide_right.png", "panel/slide_left.png"));
        button->setProperty(Qn::NoHandScrollOver, true);
        setHelpTopic(button, Qn::MainWindow_Pin_Help);
        return button;
    }

    QnImageButtonWidget *newPinButton(QGraphicsItem *parent = NULL, QAction *action = NULL) {
        QnImageButtonWidget *button = new QnImageButtonWidget(parent);
        button->resize(24, 24);
        if (action)
            button->setDefaultAction(action);
        else
            button->setCheckable(true);
        button->setIcon(qnSkin->icon("panel/pin.png", "panel/unpin.png"));
        setHelpTopic(button, Qn::MainWindow_Pin_Help);
        return button;
    }

    QGraphicsWidget *newSpacerWidget(qreal w, qreal h) {
        GraphicsWidget *result = new GraphicsWidget();
        result->setMinimumSize(QSizeF(w, h));
        result->setMaximumSize(result->minimumSize());
        return result;
    }

    class QnTopResizerWidget: public GraphicsWidget {
        typedef GraphicsWidget base_type;

    public:
        QnTopResizerWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags wFlags = 0):
            base_type(parent, wFlags)
        {
            setAcceptedMouseButtons(Qt::LeftButton);
            setCursor(Qt::SizeVerCursor);
            setAcceptHoverEvents(false);
            setFlag(ItemHasNoContents, true);
            setFlag(ItemIsMovable, true);
            setHandlingFlag(ItemHandlesMovement, true);
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

    const int hideConstrolsTimeoutMSec = 2000;
    const int closeConstrolsTimeoutMSec = 2000;

} // anonymous namespace


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
    m_windowButtonsUsed(true),
    m_ignoreClickEvent(false),
    m_inactive(false),
    m_inFreespace(false),
    m_ignoreSliderResizerGeometryChanges(false),
    m_ignoreSliderResizerGeometryChanges2(false),
    m_sliderZoomingIn(false),
    m_sliderZoomingOut(false),
    m_lastThumbnailsHeight(48.0),
    m_inCalendarGeometryUpdate(false)
{
    memset(m_widgetByRole, 0, sizeof(m_widgetByRole));

    QGraphicsLayout::setInstantInvalidatePropagation(true);

    /* Install and configure instruments. */
    m_fpsCountingInstrument = new FpsCountingInstrument(333, this);
    m_controlsActivityInstrument = new ActivityListenerInstrument(true, hideConstrolsTimeoutMSec, this);

    m_instrumentManager->installInstrument(m_fpsCountingInstrument, InstallationMode::InstallBefore, display()->paintForwardingInstrument());
    m_instrumentManager->installInstrument(m_controlsActivityInstrument);

    connect(m_controlsActivityInstrument, SIGNAL(activityStopped()),                                                                this,                           SLOT(at_activityStopped()));
    connect(m_controlsActivityInstrument, SIGNAL(activityResumed()),                                                                this,                           SLOT(at_activityStarted()));
    connect(m_fpsCountingInstrument,    SIGNAL(fpsChanged(qreal)),                                                                  this,                           SLOT(at_fpsChanged(qreal)));

    /* Create controls. */
    m_controlsWidget = new QnUiElementsWidget();
    m_controlsWidget->setAcceptedMouseButtons(0);
    display()->scene()->addItem(m_controlsWidget);
    display()->setLayer(m_controlsWidget, Qn::UiLayer);

    QnSingleEventSignalizer *deactivationSignalizer = new QnSingleEventSignalizer(this);
    deactivationSignalizer->setEventType(QEvent::WindowDeactivate);
    m_controlsWidget->installEventFilter(deactivationSignalizer);

    connect(deactivationSignalizer,     SIGNAL(activated(QObject *, QEvent *)),                                                     this,                           SLOT(at_controlsWidget_deactivated()));
    connect(m_controlsWidget,           SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_controlsWidget_geometryChanged()));


    /* Animation. */
    setTimer(m_instrumentManager->animationTimer());
    startListening();


    /* Fps counter. */
    m_fpsItem = new QnProxyLabel(m_controlsWidget);
    m_fpsItem->setAcceptedMouseButtons(0);
    m_fpsItem->setAcceptsHoverEvents(false);
    setPaletteColor(m_fpsItem, QPalette::Window, Qt::transparent);
    setPaletteColor(m_fpsItem, QPalette::WindowText,  QColor(63, 159, 216));

    display()->view()->addAction(action(Qn::ShowFpsAction));
    connect(action(Qn::ShowFpsAction),  SIGNAL(toggled(bool)),                                                                      this,                           SLOT(setFpsVisible(bool)));
    connect(m_fpsItem,                  SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_fpsItem_geometryChanged()));
    setFpsVisible(false);


    /* Tree widget. */
    m_treeWidget = new QnResourceBrowserWidget(NULL, context());
    m_treeWidget->setAttribute(Qt::WA_TranslucentBackground);
    setPaletteColor(m_treeWidget, QPalette::Window, Qt::transparent);
    setPaletteColor(m_treeWidget, QPalette::Base, Qt::transparent);
    setPaletteColor(m_treeWidget, QPalette::Text, Qt::white);
    setPaletteColor(m_treeWidget->typeComboBox(), QPalette::Window, Qt::black);
    setPaletteColor(m_treeWidget->typeComboBox(), QPalette::Base, Qt::black);
    m_treeWidget->resize(250, 0);

    m_treeBackgroundItem = new QnSimpleFrameWidget(m_controlsWidget);
    m_treeBackgroundItem->setAutoFillBackground(true);
    {
        QLinearGradient gradient(0, 0, 1, 0);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(0.0, QColor(0, 0, 0, 255));
        gradient.setColorAt(1.0, QColor(0, 0, 0, 64));
        gradient.setSpread(QGradient::RepeatSpread);

        m_treeBackgroundItem->setWindowBrush(gradient);
    }
    m_treeBackgroundItem->setFrameColor(QColor(110, 110, 110, 255));
    m_treeBackgroundItem->setFrameWidth(0.5);

    m_treeItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_treeItem->setWidget(m_treeWidget);
    m_treeWidget->installEventFilter(m_treeItem);
    m_treeItem->setFocusPolicy(Qt::StrongFocus);
    m_treeItem->setProperty(Qn::NoHandScrollOver, true);

    m_treePinButton = newPinButton(m_controlsWidget, action(Qn::PinTreeAction));
    m_treePinButton->setFocusProxy(m_treeItem);

    m_treeShowButton = newShowHideButton(m_controlsWidget, action(Qn::ToggleTreeAction));
    m_treeShowButton->setFocusProxy(m_treeItem);

    m_treeOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_treeOpacityProcessor->addTargetItem(m_treeItem);
    m_treeOpacityProcessor->addTargetItem(m_treeShowButton);

    m_treeHidingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_treeHidingProcessor->addTargetItem(m_treeItem);
    m_treeHidingProcessor->addTargetItem(m_treeShowButton);
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

    connect(m_treeWidget,               SIGNAL(selectionChanged()),                                                                 action(Qn::SelectionChangeAction), SLOT(trigger()));
    connect(m_treeWidget,               SIGNAL(activated(const QnResourcePtr &)),                                                   this,                           SLOT(at_treeWidget_activated(const QnResourcePtr &)));
    connect(m_treeOpacityProcessor,     SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateTreeOpacity()));
    connect(m_treeOpacityProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateTreeOpacity()));
    connect(m_treeOpacityProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateControlsVisibility()));
    connect(m_treeOpacityProcessor,     SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateControlsVisibility()));
    connect(m_treeHidingProcessor,      SIGNAL(hoverFocusLeft()),                                                                   this,                           SLOT(at_treeHidingProcessor_hoverFocusLeft()));
    connect(m_treeShowingProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(at_treeShowingProcessor_hoverEntered()));
    connect(m_treeItem,                 SIGNAL(paintRectChanged()),                                                                 this,                           SLOT(at_treeItem_paintGeometryChanged()));
    connect(m_treeItem,                 SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_treeItem_paintGeometryChanged()));
    connect(action(Qn::ToggleTreeAction), SIGNAL(toggled(bool)),                                                                    this,                           SLOT(at_toggleTreeAction_toggled(bool)));
    connect(action(Qn::PinTreeAction),  SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_pinTreeAction_toggled(bool)));
    connect(action(Qn::PinNotificationsAction),  SIGNAL(toggled(bool)),                                                             this,                           SLOT(at_pinNotificationsAction_toggled(bool)));


    /* Title bar. */
    m_titleBackgroundItem = new QnSimpleFrameWidget(m_controlsWidget);
    m_titleBackgroundItem->setAutoFillBackground(true);
    {
        QLinearGradient gradient(0, 0, 0, 1);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(0.0, QColor(0, 0, 0, 255));
        gradient.setColorAt(1.0, QColor(0, 0, 0, 64));
        gradient.setSpread(QGradient::RepeatSpread);

        m_titleBackgroundItem->setWindowBrush(gradient);
    }
    m_titleBackgroundItem->setFrameColor(QColor(110, 110, 110, 255));
    m_titleBackgroundItem->setFrameWidth(0.5);

    m_titleItem = new QnClickableWidget(m_controlsWidget);
    m_titleItem->setPos(0.0, 0.0);
    m_titleItem->setClickableButtons(Qt::LeftButton);
    m_titleItem->setProperty(Qn::NoHandScrollOver, true);

    QnSingleEventSignalizer *titleMenuSignalizer = new QnSingleEventSignalizer(this);
    titleMenuSignalizer->setEventType(QEvent::GraphicsSceneContextMenu);
    m_titleItem->installEventFilter(titleMenuSignalizer);

    m_tabBarItem = new QGraphicsProxyWidget(m_controlsWidget);
    m_tabBarItem->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    
    m_tabBarWidget = new QnLayoutTabBar(NULL, context());
    m_tabBarWidget->setAttribute(Qt::WA_TranslucentBackground);
    m_tabBarItem->setWidget(m_tabBarWidget);
    m_tabBarWidget->installEventFilter(m_tabBarItem);

    m_mainMenuButton = newActionButton(action(Qn::MainMenuAction), 1.5, Qn::MainWindow_TitleBar_MainMenu_Help);

    QGraphicsLinearLayout * windowButtonsLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    windowButtonsLayout->setContentsMargins(0, 0, 0, 0);
    windowButtonsLayout->setSpacing(2);
    windowButtonsLayout->addItem(newSpacerWidget(6.0, 6.0));
    windowButtonsLayout->addItem(newActionButton(action(Qn::WhatsThisAction)));
    windowButtonsLayout->addItem(newActionButton(action(Qn::MinimizeAction)));
    windowButtonsLayout->addItem(newActionButton(action(Qn::EffectiveMaximizeAction), 1.0, Qn::MainWindow_Fullscreen_Help));
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
    m_titleRightButtonsLayout->addItem(newActionButton(action(Qn::OpenNewTabAction)));
    m_titleRightButtonsLayout->addItem(newActionButton(action(Qn::OpenCurrentUserLayoutMenu)));
    m_titleRightButtonsLayout->addStretch(0x1000);
    m_titleRightButtonsLayout->addItem(newActionButton(action(Qn::TogglePanicModeAction), 1.0, Qn::MainWindow_Panic_Help));
    if (QnScreenRecorder::isSupported())
        m_titleRightButtonsLayout->addItem(newActionButton(action(Qn::ToggleScreenRecordingAction), 1.0, Qn::MainWindow_ScreenRecording_Help));
    m_titleRightButtonsLayout->addItem(newActionButton(action(Qn::ConnectToServerAction)));
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

    connect(m_tabBarWidget,             SIGNAL(closeRequested(QnWorkbenchLayout *)),                                                this,                           SLOT(at_tabBar_closeRequested(QnWorkbenchLayout *)));
    connect(m_titleOpacityProcessor,    SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateTitleOpacity()));
    connect(m_titleOpacityProcessor,    SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateTitleOpacity()));
    connect(m_titleOpacityProcessor,    SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateControlsVisibility()));
    connect(m_titleOpacityProcessor,    SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateControlsVisibility()));
    connect(m_titleItem,                SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_titleItem_geometryChanged()));
    connect(m_titleItem,                SIGNAL(doubleClicked()),                                                                    action(Qn::EffectiveMaximizeAction), SLOT(toggle()));
    connect(titleMenuSignalizer,        SIGNAL(activated(QObject *, QEvent *)),                                                     this,                           SLOT(at_titleItem_contextMenuRequested(QObject *, QEvent *)));
    connect(action(Qn::ToggleTitleBarAction), SIGNAL(toggled(bool)),                                                                this,                           SLOT(at_toggleTitleBarAction_toggled(bool)));


    /* Notifications window. */
    m_notificationsBackgroundItem = new QnSimpleFrameWidget(m_controlsWidget);
    m_notificationsBackgroundItem->setAutoFillBackground(true);
    {
        QLinearGradient gradient(0, 0, 1, 0);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(1.0, QColor(0, 0, 0, 255));
        gradient.setColorAt(0.0, QColor(0, 0, 0, 64));
        gradient.setSpread(QGradient::RepeatSpread);

        m_notificationsBackgroundItem->setWindowBrush(gradient);
    }
    m_notificationsBackgroundItem->setFrameColor(QColor(110, 110, 110, 255));
    m_notificationsBackgroundItem->setFrameWidth(0.5);

    m_notificationsItem = new QnNotificationsCollectionWidget(m_controlsWidget, 0, context());
    m_notificationsItem->setProperty(Qn::NoHandScrollOver, true);

    m_notificationsPinButton = newPinButton(m_controlsWidget, action(Qn::PinNotificationsAction));
    m_notificationsPinButton->setFocusProxy(m_notificationsItem);

    m_notificationsShowButton = newShowHideButton(m_controlsWidget);
    m_notificationsShowButton->setTransform(QTransform::fromScale(-1, 1));
    m_notificationsShowButton->setFocusProxy(m_notificationsItem);
    m_notificationsShowButton->stackBefore(m_notificationsItem);

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
    m_notificationsXAnimator->setSpeed(m_notificationsItem->size().width() * 2.0);
    m_notificationsXAnimator->setTimeLimit(500);

    m_notificationsOpacityAnimatorGroup = new AnimatorGroup(this);
    m_notificationsOpacityAnimatorGroup->setTimer(m_instrumentManager->animationTimer());
    m_notificationsOpacityAnimatorGroup->addAnimator(opacityAnimator(m_notificationsItem));
    m_notificationsOpacityAnimatorGroup->addAnimator(opacityAnimator(m_notificationsBackgroundItem)); /* Speed of 1.0 is OK here. */
    m_notificationsOpacityAnimatorGroup->addAnimator(opacityAnimator(m_notificationsShowButton));
    m_notificationsOpacityAnimatorGroup->addAnimator(opacityAnimator(m_notificationsPinButton));

    connect(m_notificationsPinButton,           SIGNAL(toggled(bool)),          this,   SLOT(at_notificationsPinButton_toggled(bool)));
    connect(m_notificationsShowButton,          SIGNAL(toggled(bool)),          this,   SLOT(at_notificationsShowButton_toggled(bool)));
    connect(m_notificationsOpacityProcessor,    SIGNAL(hoverLeft()),            this,   SLOT(updateNotificationsOpacity()));
    connect(m_notificationsOpacityProcessor,    SIGNAL(hoverEntered()),         this,   SLOT(updateNotificationsOpacity()));
    connect(m_notificationsOpacityProcessor,    SIGNAL(hoverEntered()),         this,   SLOT(updateControlsVisibility()));
    connect(m_notificationsOpacityProcessor,    SIGNAL(hoverLeft()),            this,   SLOT(updateControlsVisibility()));
    connect(m_notificationsHidingProcessor,     SIGNAL(hoverFocusLeft()),       this,   SLOT(at_notificationsHidingProcessor_hoverFocusLeft()));
    connect(m_notificationsShowingProcessor,    SIGNAL(hoverEntered()),         this,   SLOT(at_notificationsShowingProcessor_hoverEntered()));
    connect(m_notificationsItem,                SIGNAL(geometryChanged()),      this,   SLOT(at_notificationsItem_geometryChanged()));
    connect(m_notificationsItem,                SIGNAL(visibleSizeChanged()),   this,   SLOT(at_notificationsItem_geometryChanged()));
    connect(m_notificationsItem,                SIGNAL(sizeHintChanged()),      this,   SLOT(updateNotificationsGeometry()));

    /* Calendar. */
    QnCalendarWidget *calendarWidget = new QnCalendarWidget();
    navigator()->setCalendar(calendarWidget);
    connect(calendarWidget, SIGNAL(emptyChanged()), this, SLOT(updateCalendarVisibility()));

    m_calendarItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_calendarItem->setWidget(calendarWidget);
    calendarWidget->installEventFilter(m_calendarItem);
    m_calendarItem->resize(250, 200);
    m_calendarItem->setProperty(Qn::NoHandScrollOver, true);

    m_calendarShowButton = newShowHideButton(m_controlsWidget);
    {
        QTransform transform;
        transform.rotate(-90);
        m_calendarShowButton->setTransform(transform);
    }
    m_calendarShowButton->setFocusProxy(m_calendarItem);
    m_calendarShowButton->setVisible(false);

    m_calendarOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_calendarOpacityProcessor->addTargetItem(m_calendarItem);
    m_calendarOpacityProcessor->addTargetItem(m_calendarShowButton);

    m_calendarHidingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_calendarHidingProcessor->addTargetItem(m_calendarItem);
    m_calendarHidingProcessor->addTargetItem(m_calendarShowButton);
    m_calendarHidingProcessor->setHoverLeaveDelay(closeConstrolsTimeoutMSec);
    m_calendarHidingProcessor->setFocusLeaveDelay(closeConstrolsTimeoutMSec);

    m_calendarSizeAnimator = new VariantAnimator(this);
    m_calendarSizeAnimator->setTimer(m_instrumentManager->animationTimer());
    m_calendarSizeAnimator->setTargetObject(m_calendarItem);
    m_calendarSizeAnimator->setAccessor(new PropertyAccessor("paintSize"));
    m_calendarSizeAnimator->setSpeed(100.0 * 2.0);
    m_calendarSizeAnimator->setTimeLimit(500);

    m_calendarOpacityAnimatorGroup = new AnimatorGroup(this);
    m_calendarOpacityAnimatorGroup->setTimer(m_instrumentManager->animationTimer());
    m_calendarOpacityAnimatorGroup->addAnimator(opacityAnimator(m_calendarItem));
    m_calendarOpacityAnimatorGroup->addAnimator(opacityAnimator(m_calendarShowButton)); /* Speed of 1.0 is OK here. */

    connect(m_calendarShowButton,       SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_calendarShowButton_toggled(bool)));
    connect(m_calendarOpacityProcessor, SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateCalendarOpacity()));
    connect(m_calendarOpacityProcessor, SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateCalendarOpacity()));
    connect(m_calendarOpacityProcessor, SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateControlsVisibility()));
    connect(m_calendarOpacityProcessor, SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateControlsVisibility()));
    connect(m_calendarHidingProcessor,  SIGNAL(hoverLeft()),                                                                        this,                           SLOT(at_calendarHidingProcessor_hoverFocusLeft()));
    connect(m_calendarItem,             SIGNAL(paintRectChanged()),                                                                 this,                           SLOT(at_calendarItem_paintGeometryChanged()));
    connect(m_calendarItem,             SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_calendarItem_paintGeometryChanged()));
    connect(action(Qn::ToggleCalendarAction), SIGNAL(toggled(bool)),                                                                this,                           SLOT(at_toggleCalendarAction_toggled(bool)));


    /* Navigation slider. */
    m_sliderResizerItem = new QnTopResizerWidget(m_controlsWidget);
    m_sliderResizerItem->setProperty(Qn::NoHandScrollOver, true);

    m_sliderItem = new QnNavigationItem(m_controlsWidget);
    m_sliderItem->setFrameColor(QColor(110, 110, 110, 255));
    m_sliderItem->setFrameWidth(0.5);
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
    m_sliderResizerItem->stackBefore(m_sliderShowButton);
    m_sliderResizerItem->stackBefore(m_sliderZoomButtonsWidget);
    m_sliderItem->stackBefore(m_sliderResizerItem);

    m_sliderOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_sliderOpacityProcessor->addTargetItem(m_sliderItem);
    m_sliderOpacityProcessor->addTargetItem(m_sliderItem->timeSlider()->toolTipItem());
    m_sliderOpacityProcessor->addTargetItem(m_sliderShowButton);
    m_sliderOpacityProcessor->addTargetItem(m_sliderResizerItem);
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

    connect(sliderZoomInButton,         SIGNAL(pressed()),                          this,           SLOT(at_sliderZoomInButton_pressed()));
    connect(sliderZoomInButton,         SIGNAL(released()),                         this,           SLOT(at_sliderZoomInButton_released()));
    connect(sliderZoomOutButton,        SIGNAL(pressed()),                          this,           SLOT(at_sliderZoomOutButton_pressed()));
    connect(sliderZoomOutButton,        SIGNAL(released()),                         this,           SLOT(at_sliderZoomOutButton_released()));

    connect(m_sliderOpacityProcessor,   SIGNAL(hoverEntered()),                     this,           SLOT(updateSliderOpacity()));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverLeft()),                        this,           SLOT(updateSliderOpacity()));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverEntered()),                     this,           SLOT(updateControlsVisibility()));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverLeft()),                        this,           SLOT(updateControlsVisibility()));
    connect(m_sliderItem,               SIGNAL(geometryChanged()),                  this,           SLOT(at_sliderItem_geometryChanged()));
    connect(m_sliderResizerItem,        SIGNAL(geometryChanged()),                  this,           SLOT(at_sliderResizerItem_geometryChanged()));
    connect(navigator(),                SIGNAL(currentWidgetChanged()),             this,           SLOT(updateControlsVisibility()));
    connect(action(Qn::ToggleTourModeAction), SIGNAL(toggled(bool)),                this,           SLOT(updateControlsVisibility()));
    connect(action(Qn::ToggleThumbnailsAction), SIGNAL(toggled(bool)),              this,           SLOT(at_toggleThumbnailsAction_toggled(bool)));
    connect(action(Qn::ToggleSliderAction), SIGNAL(toggled(bool)),                  this,           SLOT(at_toggleSliderAction_toggled(bool)));

    initGraphicsMessageBox();

    /* Connect to display. */
    display()->view()->addAction(action(Qn::FreespaceAction));
    connect(action(Qn::FreespaceAction),SIGNAL(triggered()),                        this,           SLOT(at_freespaceAction_triggered()));
    connect(action(Qn::FullscreenAction),SIGNAL(triggered()),                       this,           SLOT(at_fullscreenAction_triggered()));
    connect(display(),                  SIGNAL(viewportGrabbed()),                  this,           SLOT(disableProxyUpdates()));
    connect(display(),                  SIGNAL(viewportUngrabbed()),                this,           SLOT(enableProxyUpdates()));
    connect(display(),                  SIGNAL(widgetChanged(Qn::ItemRole)),        this,           SLOT(at_display_widgetChanged(Qn::ItemRole)));


    /* Init fields. */
    setFlags(HideWhenNormal | HideWhenZoomed | AdjustMargins);

    setSliderVisible(false, false);
    setTreeVisible(true, false);
    setTitleVisible(true, false);
    setTitleUsed(false);
    setNotificationsVisible(true, false);
    setCalendarOpened(false, false);
    setCalendarVisible(false);
    updateControlsVisibility(false);

    //TODO: #GDM think about a refactoring
    bool treeOpened = qnSettings->isTreeOpened(); //quite a hack because m_treePinButton sets tree opened if it is pinned
    bool notificationsOpened = qnSettings->isNotificationsOpened(); //same shit
    m_treePinButton->setChecked(qnSettings->isTreePinned());
    m_notificationsPinButton->setChecked(qnSettings->isNotificationsPinned());
    setTreeOpened(treeOpened, false);
    setTitleOpened(qnSettings->isTitleOpened(), false, false);
    setSliderOpened(qnSettings->isSliderOpened(), false, false);
    setNotificationsOpened(notificationsOpened, false);

    /* Set up title D&D. */
    DropInstrument *dropInstrument = new DropInstrument(true, context(), this);
    display()->instrumentManager()->installInstrument(dropInstrument);
    dropInstrument->setSurface(m_titleBackgroundItem);

    /* Set up notifications context processing. */
    m_motionDisplayWatcher = context()->instance<QnWorkbenchMotionDisplayWatcher>();
    connect(display()->focusListenerInstrument(), SIGNAL(focusItemChanged()),                                                       this,                           SLOT(updateHelpContext()));
    connect(m_treeWidget,               SIGNAL(currentTabChanged()),                                                                this,                           SLOT(updateHelpContext()));
    connect(m_motionDisplayWatcher,     SIGNAL(motionGridShown()),                                                                  this,                           SLOT(updateHelpContext()));
    connect(m_motionDisplayWatcher,     SIGNAL(motionGridHidden()),                                                                 this,                           SLOT(updateHelpContext()));
    updateHelpContext();
}

QnWorkbenchUi::~QnWorkbenchUi() {
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

void QnWorkbenchUi::setTreeOpened(bool opened, bool animate, bool save) {
    m_inFreespace = false;

    m_treeShowingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    qreal newX = opened ? 0.0 : -m_treeItem->size().width() - 1.0 /* Just in case. */;
    if (animate) {
        m_treeXAnimator->animateTo(newX);
    } else {
        m_treeXAnimator->stop();
        m_treeItem->setX(newX);
    }

    QnScopedValueRollback<bool> rollback(&m_ignoreClickEvent, true);
    action(Qn::ToggleTreeAction)->setChecked(opened);
    Q_UNUSED(rollback)

    if (save)
        qnSettings->setTreeOpened(opened);
}

void QnWorkbenchUi::setSliderOpened(bool opened, bool animate, bool save) {
    m_inFreespace = false;

    qreal newY = m_controlsWidgetRect.bottom() + (opened ? -m_sliderItem->size().height() : 48.0 /* So that tooltips are not opened. */);
    if (animate) {
        m_sliderYAnimator->animateTo(newY);
    } else {
        m_sliderYAnimator->stop();
        m_sliderItem->setY(newY);
    }

    updateCalendarVisibility(animate);

    QnScopedValueRollback<bool> rollback(&m_ignoreClickEvent, true);
    action(Qn::ToggleSliderAction)->setChecked(opened);
    Q_UNUSED(rollback)

    if (save)
        qnSettings->setSliderOpened(opened);
}

void QnWorkbenchUi::setTitleOpened(bool opened, bool animate, bool save) {
    m_inFreespace = false;

    QnScopedValueRollback<bool> rollback(&m_ignoreClickEvent, true);
    action(Qn::ToggleTitleBarAction)->setChecked(opened);
    Q_UNUSED(rollback)

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

void QnWorkbenchUi::setNotificationsOpened(bool opened, bool animate, bool save) {
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

    QnScopedValueRollback<bool> rollback(&m_ignoreClickEvent, true);
    m_notificationsShowButton->setChecked(opened);

    if (save)
        qnSettings->setNotificationsOpened(opened);
}

void QnWorkbenchUi::setCalendarOpened(bool opened, bool animate) {
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

    QnScopedValueRollback<bool> rollback(&m_ignoreClickEvent, true);
    m_calendarShowButton->setChecked(opened);
}

void QnWorkbenchUi::setTreeVisible(bool visible, bool animate) {
    bool changed = m_treeVisible != visible;

    m_treeVisible = visible;

    updateTreeOpacity(animate);
    if(changed)
        updateTreeGeometry();
}

void QnWorkbenchUi::setSliderVisible(bool visible, bool animate) {
    bool changed = m_sliderVisible != visible;

    m_sliderVisible = visible;

    updateSliderOpacity(animate);
    if(changed) {
        updateTreeGeometry();
        updateNotificationsGeometry();
        updateCalendarVisibility(animate);

        m_sliderItem->setEnabled(m_sliderVisible); /* So that it doesn't handle mouse events while disappearing. */
    }
}

void QnWorkbenchUi::setTitleVisible(bool visible, bool animate) {
    bool changed = m_titleVisible != visible;

    m_titleVisible = visible;

    updateTitleOpacity(animate);
    if(changed) {
        updateTreeGeometry();
        updateNotificationsGeometry();
    }
}

void QnWorkbenchUi::setNotificationsVisible(bool visible, bool animate) {
    bool changed = m_notificationsVisible != visible;

    m_notificationsVisible = visible;

    updateNotificationsOpacity(animate);
    if(changed)
        updateNotificationsGeometry();
}

void QnWorkbenchUi::setCalendarVisible(bool visible, bool animate) {
    bool changed = m_calendarVisible != visible;

    m_calendarVisible = visible;

    updateCalendarOpacity(animate);
    if(changed) {
        updateNotificationsGeometry();
    }
}

void QnWorkbenchUi::setProxyUpdatesEnabled(bool updatesEnabled) {
    m_treeItem->setUpdatesEnabled(updatesEnabled);
}

void QnWorkbenchUi::setTitleUsed(bool used) {
    m_titleItem->setVisible(used);
    m_titleBackgroundItem->setVisible(used);
    m_titleShowButton->setVisible(used);

    if(used) {
        m_titleUsed = used;

        setTitleOpened(isTitleOpened(), false);

        at_titleItem_geometryChanged();

        /* For reasons unknown, tab bar's size gets messed up when it is shown 
         * after new items were added to it. Re-embedding notificationss, probably there is
         * a simpler workaround. */
        QTabBar *widget = checked_cast<QTabBar *>(m_tabBarItem->widget());
        m_tabBarItem->setWidget(NULL);
        m_tabBarItem->setWidget(widget);
        widget->installEventFilter(m_tabBarItem);

        /* There are cases where even re-embedding doesn't notifications.
         * So we cheat even more, forcing the tab bar to refresh. */
        QTabBar::Shape shape = widget->shape();
        widget->setShape(QTabBar::TriangularWest);
        widget->setShape(shape);
    } else {
        m_titleItem->setPos(0.0, -m_titleItem->size().height() - 1.0);

        m_titleUsed = used;
    }
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

void QnWorkbenchUi::setTreeOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate) {
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
}

void QnWorkbenchUi::setSliderOpacity(qreal opacity, bool animate) {
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

    m_sliderResizerItem->setVisible(!qFuzzyIsNull(opacity));
}

void QnWorkbenchUi::setTitleOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate) {
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

void QnWorkbenchUi::setNotificationsOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate) {
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

void QnWorkbenchUi::setCalendarOpacity(qreal opacity, bool animate) {
    if(animate) {
        m_calendarOpacityAnimatorGroup->pause();
        opacityAnimator(m_calendarItem)->setTargetValue(opacity);
        opacityAnimator(m_calendarShowButton)->setTargetValue(opacity);
        m_calendarOpacityAnimatorGroup->start();
    } else {
        m_calendarOpacityAnimatorGroup->stop();
        m_calendarItem->setOpacity(opacity);
        m_calendarShowButton->setOpacity(opacity);
    }
}

void QnWorkbenchUi::setZoomButtonsOpacity(qreal opacity, bool animate) {
    if(animate) {
        opacityAnimator(m_sliderZoomButtonsWidget)->animateTo(opacity);
    } else {
        m_sliderZoomButtonsWidget->setOpacity(opacity);
    }
}

void QnWorkbenchUi::updateTreeOpacity(bool animate) {
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

void QnWorkbenchUi::updateSliderOpacity(bool animate) {
    if(!m_sliderVisible) {
        setSliderOpacity(0.0, animate);
        setZoomButtonsOpacity(0.0, animate);
    } else {
        if(m_sliderOpacityProcessor->isHovered()) {
            setSliderOpacity(hoverSliderOpacity, animate);
            setZoomButtonsOpacity(hoverSliderOpacity, animate);
        } else {
            setSliderOpacity(normalSliderOpacity, animate);
            setZoomButtonsOpacity(0.0, animate);
        }
    }
}

void QnWorkbenchUi::updateTitleOpacity(bool animate) {
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

void QnWorkbenchUi::updateNotificationsOpacity(bool animate) {
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

void QnWorkbenchUi::updateCalendarOpacity(bool animate) {
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
    bool calendarEmpty = true;
    if (QnCalendarWidget* c = dynamic_cast<QnCalendarWidget *>(m_calendarItem->widget()))
        calendarEmpty = c->isEmpty(); /* Small hack. We have a signal that updates visibility if a calendar receive new data */

    bool calendarEnabled = !calendarEmpty && (navigator()->currentWidget() && navigator()->currentWidget()->resource()->flags() & QnResource::utc);
    action(Qn::ToggleCalendarAction)->setEnabled(calendarEnabled); // TODO: #GDM does this belong here?

    bool calendarVisible = calendarEnabled && m_sliderVisible && isSliderOpened();

    if(m_inactive) {
        bool hovered = m_sliderOpacityProcessor->isHovered() || m_treeOpacityProcessor->isHovered() || m_titleOpacityProcessor->isHovered() || m_notificationsOpacityProcessor->isHovered() || m_calendarOpacityProcessor->isHovered();
        setCalendarVisible(calendarVisible && hovered, animate);
    } else {
        setCalendarVisible(calendarVisible, animate);
    }
}

void QnWorkbenchUi::updateControlsVisibility(bool animate) {    // TODO
    bool sliderVisible = 
        navigator()->currentWidget() != NULL && 
        !(navigator()->currentWidget()->resource()->flags() & (QnResource::still_image | QnResource::server)) &&
        ((accessController()->globalPermissions() & Qn::GlobalViewArchivePermission) || !(navigator()->currentWidget()->resource()->flags() & QnResource::live)) &&
        !action(Qn::ToggleTourModeAction)->isChecked();

    if(m_inactive) {
        bool hovered = m_sliderOpacityProcessor->isHovered() || m_treeOpacityProcessor->isHovered() || m_titleOpacityProcessor->isHovered() || m_notificationsOpacityProcessor->isHovered() || m_calendarOpacityProcessor->isHovered();
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
        defer |= !qFuzzyCompare(sliderPos, m_sliderItem->pos()); /* If animation is running, then geometry sync should be deferred. */
    } else {
        sliderPos = m_sliderItem->pos();
    }

    /* Calculate title target position. */
    QPointF titlePos;
    if((!m_titleVisible || !m_titleUsed) && m_treeVisible) {
        titlePos = QPointF(m_titleItem->pos().x(), -m_titleItem->size().height());
    } else if(m_titleYAnimator->isRunning()) {
        titlePos = QPointF(m_titleItem->pos().x(), m_titleYAnimator->targetValue().toReal());
        defer |= !qFuzzyCompare(titlePos, m_titleItem->pos());
    } else {
        titlePos = m_titleItem->pos();
    }

    /* Calculate target geometry. */
    geometry = updatedTreeGeometry(m_treeItem->geometry(), QRectF(titlePos, m_titleItem->size()), QRectF(sliderPos, m_sliderItem->size()));
    if(qFuzzyCompare(geometry, m_treeItem->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if(defer && geometry.height() < m_treeItem->size().height())
        return;

    m_treeItem->resize(geometry.size());
}

QRectF QnWorkbenchUi::updatedNotificationsGeometry(const QRectF &notificationsGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry, const QRectF &calendarGeometry, qreal *maxHeight) {
    QPointF pos(
        notificationsGeometry.x(),
        ((!m_titleVisible || !m_titleUsed) && m_notificationsVisible) ? 30.0 : qMax(titleGeometry.bottom() + 30.0, 30.0)
    );

    *maxHeight = qMin(
                m_sliderVisible ? sliderGeometry.y() - 30.0 : m_controlsWidgetRect.bottom() - 30.0,
                m_calendarVisible ? calendarGeometry.y() - 30.0 : m_controlsWidgetRect.bottom() - 30.0
            ) - pos.y();
    qreal preferredHeight = m_notificationsItem->preferredHeight();
    QSizeF size(notificationsGeometry.width(), qMin(*maxHeight, preferredHeight));
    return QRectF(pos, size);
}

void QnWorkbenchUi::updateNotificationsGeometry() {
    qreal maxHeight = 0;

    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedNotificationsGeometry(m_notificationsItem->geometry(), m_titleItem->geometry(), m_sliderItem->geometry(), m_calendarItem->paintGeometry(), &maxHeight);
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
        defer |= !qFuzzyCompare(sliderPos, m_sliderItem->pos()); /* If animation is running, then geometry sync should be deferred. */
    } else {
        sliderPos = m_sliderItem->pos();
    }

    /* Calculate calendar target position. */
    QPointF calendarPos;
    if(!m_calendarVisible && m_notificationsVisible) {
        calendarPos = QPointF(m_calendarItem->pos().x(), m_controlsWidgetRect.bottom());
    } else if(m_calendarSizeAnimator->isRunning()) {
        calendarPos = QPointF(m_calendarItem->pos().x(), sliderPos.y() - m_calendarSizeAnimator->targetValue().toSizeF().height());
        defer |= !qFuzzyCompare(calendarPos, m_calendarItem->pos()); /* If animation is running, then geometry sync should be deferred. */
    } else {
        calendarPos = m_calendarItem->pos();
    }

    /* Calculate title target position. */
    QPointF titlePos;
    if((!m_titleVisible || !m_titleUsed) && m_notificationsVisible) {
        titlePos = QPointF(m_titleItem->pos().x(), -m_titleItem->size().height());
    } else if(m_titleYAnimator->isRunning()) {
        titlePos = QPointF(m_titleItem->pos().x(), m_titleYAnimator->targetValue().toReal());
        defer |= !qFuzzyCompare(titlePos, m_titleItem->pos());
    } else {
        titlePos = m_titleItem->pos();
    }

    /* Calculate target geometry. */
    geometry = updatedNotificationsGeometry(m_notificationsItem->geometry(),
                                            QRectF(titlePos, m_titleItem->size()),
                                            QRectF(sliderPos, m_sliderItem->size()),
                                            QRectF(calendarPos, m_calendarItem->paintSize()),
                                            &maxHeight);
    if(qFuzzyCompare(geometry, m_notificationsItem->geometry()))
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
    qDebug() << "tooltipEncRect" << tooltipsEnclosingRect;
    qDebug() << "to scene" << m_controlsWidget->mapRectToScene(tooltipsEnclosingRect);

    m_notificationsItem->setToolTipsEnclosingRect(m_controlsWidget->mapRectToItem(m_notificationsItem, tooltipsEnclosingRect));
}

QRectF QnWorkbenchUi::updatedCalendarGeometry(const QRectF &sliderGeometry) {
    QRectF geometry = m_calendarItem->paintGeometry();
    geometry.moveLeft(m_controlsWidgetRect.right() - geometry.width());
    geometry.moveBottom(sliderGeometry.top());
    return geometry;
}

void QnWorkbenchUi::updateCalendarGeometry() {
    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedCalendarGeometry(m_sliderItem->geometry());
    m_calendarItem->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_calendarItem->setPos(geometry.topLeft());

    /* Whether actual size change should be deferred. */
    bool defer = m_calendarSizeAnimator->isRunning();

    /* Calculate slider target position. */
    QPointF sliderPos;
    if(!m_sliderVisible && m_calendarVisible) {
        sliderPos = QPointF(m_sliderItem->pos().x(), m_controlsWidgetRect.bottom());
    } else if(m_sliderYAnimator->isRunning()) {
        sliderPos = QPointF(m_sliderItem->pos().x(), m_sliderYAnimator->targetValue().toReal());
        defer |= !qFuzzyCompare(sliderPos, m_sliderItem->pos()); /* If animation is running, then geometry sync should be deferred. */
    } else {
        sliderPos = m_sliderItem->pos();
    }

    /* Don't change size. */
}

void QnWorkbenchUi::updateFpsGeometry() {
    QPointF pos = QPointF(
        m_controlsWidgetRect.right() - m_fpsItem->size().width(),
        m_titleItem->geometry().bottom()
    );

    if(qFuzzyCompare(pos, m_fpsItem->pos()))
        return;

    m_fpsItem->setPos(pos);
}

void QnWorkbenchUi::updateSliderResizerGeometry() {
    if(m_ignoreSliderResizerGeometryChanges2) {
        QMetaObject::invokeMethod(this, "updateSliderResizerGeometry", Qt::QueuedConnection);
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
    
    if(!qFuzzyCompare(sliderResizerGeometry, m_sliderResizerItem->geometry())) {
        QnScopedValueRollback<bool> guard(&m_ignoreSliderResizerGeometryChanges2, true);

        m_sliderResizerItem->setGeometry(sliderResizerGeometry);

        /* This one is needed here as we're in a handler and thus geometry change doesn't adjust position =(. */
        m_sliderResizerItem->setPos(sliderResizerGeometry.topLeft());  // TODO: #Elric remove this ugly hack.
    }
}

void QnWorkbenchUi::updateSliderZoomButtonsGeometry() {
    QPointF pos = m_sliderItem->timeSlider()->mapToItem(m_controlsWidget, m_sliderItem->timeSlider()->rect().topLeft());

    m_sliderZoomButtonsWidget->setPos(pos);
}

QMargins QnWorkbenchUi::calculateViewportMargins(qreal treeX, qreal treeW, qreal titleY, qreal titleH, qreal sliderY, qreal notificationsX) {
    return QMargins(
        isTreePinned() ? std::floor(qMax(0.0, treeX + treeW)) : 0.0,
        std::floor(qMax(0.0, titleY + titleH)),
        std::floor(qMax(0.0, m_notificationsPinned ? m_controlsWidgetRect.right() - notificationsX : 0.0)),
        std::floor(qMax(0.0, m_controlsWidgetRect.bottom() - sliderY))
    );
}

bool QnWorkbenchUi::isFpsVisible() const {
    return m_fpsItem->isVisible();
}

void QnWorkbenchUi::setFpsVisible(bool fpsVisible) {
    if(fpsVisible == isFpsVisible())
        return;

    m_fpsItem->setVisible(fpsVisible);

    if(fpsVisible)
        m_fpsCountingInstrument->recursiveEnable();
    else
        m_fpsCountingInstrument->recursiveDisable();

    m_fpsItem->setText(QString());

    action(Qn::ShowFpsAction)->setChecked(fpsVisible);
}

bool QnWorkbenchUi::isTreeOpened() const {
    return action(Qn::ToggleTreeAction)->isChecked();
}

bool QnWorkbenchUi::isTreePinned() const {
    return action(Qn::PinTreeAction)->isChecked();
}

bool QnWorkbenchUi::isSliderOpened() const {
    return action(Qn::ToggleSliderAction)->isChecked();
}

bool QnWorkbenchUi::isTitleOpened() const {
    return action(Qn::ToggleTitleBarAction)->isChecked();
}

bool QnWorkbenchUi::isNotificationsOpened() const {
    return m_notificationsOpened;
}

void QnWorkbenchUi::setTreeShowButtonUsed(bool used) {
    if(used) {
        m_treeShowButton->setAcceptedMouseButtons(Qt::LeftButton);
    } else {
        m_treeShowButton->setAcceptedMouseButtons(0);
    }
}

void QnWorkbenchUi::setNotificationsShowButtonUsed(bool used) {
    if(used) {
        m_notificationsShowButton->setAcceptedMouseButtons(Qt::LeftButton);
    } else {
        m_notificationsShowButton->setAcceptedMouseButtons(0);
    }
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
            m_notificationsXAnimator->isRunning() ? m_notificationsXAnimator->targetValue().toReal() : m_notificationsItem->pos().x()
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

void QnWorkbenchUi::updateHelpContext() {
#if 0
    Qn::ActionScope scope = Qn::InvalidScope;

    QGraphicsItem *focusItem = display()->scene()->focusItem();

    if(focusItem == NULL || dynamic_cast<QnResourceWidget *>(focusItem)) {
        scope = Qn::SceneScope;
    } else if(focusItem == m_notificationsItem || m_notificationsItem->isAncestorOf(focusItem) || focusItem == m_titleItem || m_titleItem->isAncestorOf(focusItem)) {
        return; /* Focusing on notifications widget or title item shouldn't change notifications context. */
    } else if(focusItem == m_treeItem || m_treeItem->isAncestorOf(focusItem)) {
        scope = Qn::TreeScope;
    } else if(focusItem == m_sliderItem || m_sliderItem->isAncestorOf(focusItem)) {
        scope = Qn::SliderScope;
    } else {
        return;
    }

    QnContextHelp::ContextId context;
    switch(scope) {
    case Qn::TreeScope:
        context = QnContextHelp::ContextId_Tree;
        break;
    case Qn::SliderScope:
        context = QnContextHelp::ContextId_Slider;
        break;
    case Qn::SceneScope:
        if(m_motionDisplayWatcher->isMotionGridDisplayed()) {
            context = QnContextHelp::ContextId_MotionGrid;
        } else {
            context = QnContextHelp::ContextId_Scene;
        }
        break;
    default:
        context = QnContextHelp::ContextId_Invalid;
        break;
    }

    qnNotifications->setNotificationsContext(context);
#endif
}

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

QnWorkbenchUi::Panels QnWorkbenchUi::openedPanels() const {
    return 
        (isTreeOpened() ? TreePanel : NoPanel) |
        (isTitleOpened() ? TitlePanel : NoPanel) |
        (isSliderOpened() ? SliderPanel : NoPanel) |
        (isNotificationsOpened() ? NotificationsPanel : NoPanel);
}

void QnWorkbenchUi::setOpenedPanels(Panels panels, bool animate, bool save) {
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

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnWorkbenchUi::event(QEvent *event) {
    bool result = base_type::event(event);

    if(event->type() == QnSystemMenuEvent::SystemMenu) {
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

        m_inFreespace = false;
    }
}

void QnWorkbenchUi::at_fullscreenAction_triggered() {
    if(m_inFreespace)
        at_freespaceAction_triggered();
}

void QnWorkbenchUi::at_fpsChanged(qreal fps) {
    m_fpsItem->setText(QString::number(fps, 'g', 4));
    m_fpsItem->resize(m_fpsItem->effectiveSizeHint(Qt::PreferredSize));
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
}

void QnWorkbenchUi::at_controlsWidget_deactivated() {
    /* Re-activate it. */
    display()->scene()->setActiveWindow(m_controlsWidget);
}

void QnWorkbenchUi::at_controlsWidget_geometryChanged() {
    QGraphicsWidget *controlsWidget = m_controlsWidget;
    QRectF rect = controlsWidget->rect();
    if(qFuzzyCompare(m_controlsWidgetRect, rect))
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

    m_notificationsItem->setX(m_notificationsItem->x() - oldRect.width() + rect.width());

    updateTreeGeometry();
    updateNotificationsGeometry();
    updateFpsGeometry();
}

void QnWorkbenchUi::at_sliderItem_geometryChanged() {
    setSliderOpened(isSliderOpened(), m_sliderYAnimator->isRunning()); /* Re-adjust to screen sides. */

    updateTreeGeometry();
    updateNotificationsGeometry();

    updateViewportMargins();
    updateSliderResizerGeometry();
    updateCalendarGeometry();
    updateSliderZoomButtonsGeometry();

    QRectF geometry = m_sliderItem->geometry();
    m_sliderShowButton->setPos(QPointF(
        (geometry.left() + geometry.right() - m_titleShowButton->size().height()) / 2,
        qMin(m_controlsWidgetRect.bottom(), geometry.top())
    ));
}

void QnWorkbenchUi::at_toggleThumbnailsAction_toggled(bool checked) {
    setThumbnailsVisible(checked);
}

void QnWorkbenchUi::at_toggleCalendarAction_toggled(bool checked){
    setCalendarOpened(checked);
}

void QnWorkbenchUi::at_toggleSliderAction_toggled(bool checked) {
    if (!m_ignoreClickEvent)
        setSliderOpened(checked);
}

void QnWorkbenchUi::at_sliderResizerItem_geometryChanged() {
    if(m_ignoreSliderResizerGeometryChanges)
        return;

    QRectF sliderGeometry = m_sliderItem->geometry();

    qreal targetHeight = sliderGeometry.bottom() - m_sliderResizerItem->geometry().center().y();
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

        QnScopedValueRollback<bool> guard(&m_ignoreSliderResizerGeometryChanges, true);
        m_sliderItem->setGeometry(sliderGeometry);
    }

    updateSliderResizerGeometry();

    action(Qn::ToggleThumbnailsAction)->setChecked(isThumbnailsVisible());
}

void QnWorkbenchUi::at_treeWidget_activated(const QnResourcePtr &resource) {
    if(resource.isNull())
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
        paintGeometry.right() - m_treePinButton->size().width(),
        paintGeometry.top()
    ));

    updateViewportMargins();
}

void QnWorkbenchUi::at_treeHidingProcessor_hoverFocusLeft() {
    if(!isTreePinned())
        setTreeOpened(false);
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

void QnWorkbenchUi::at_treeShowButton_toggled(bool checked) {
    if(!m_ignoreClickEvent)
        setTreeOpened(checked);
}

void QnWorkbenchUi::at_toggleTreeAction_toggled(bool checked) {
    if (!m_ignoreClickEvent)
        setTreeOpened(checked);
}

void QnWorkbenchUi::at_pinTreeAction_toggled(bool checked) {
    if(checked)
        setTreeOpened(true);

    updateViewportMargins();

    qnSettings->setTreePinned(checked);
}

void QnWorkbenchUi::at_pinNotificationsAction_toggled(bool checked) {
    if (checked)
        setNotificationsOpened(true);
    updateViewportMargins();

    qnSettings->setNotificationsPinned(checked);
}

void QnWorkbenchUi::at_tabBar_closeRequested(QnWorkbenchLayout *layout) {
    QnWorkbenchLayoutList layouts;
    layouts.push_back(layout);
    menu()->trigger(Qn::CloseLayoutAction, layouts);
}

void QnWorkbenchUi::at_toggleTitleBarAction_toggled(bool checked) {
    if (!m_ignoreClickEvent)
        setTitleOpened(checked);
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

void QnWorkbenchUi::at_fpsItem_geometryChanged() {
    updateFpsGeometry();
}

void QnWorkbenchUi::at_notificationsPinButton_toggled(bool checked) {
    m_notificationsPinned = checked;

    if(checked)
        setNotificationsOpened(true);

    updateViewportMargins();
}

void QnWorkbenchUi::at_notificationsShowButton_toggled(bool checked) {
    if(!m_ignoreClickEvent)
        setNotificationsOpened(checked);
}

void QnWorkbenchUi::at_notificationsHidingProcessor_hoverFocusLeft() {
    if(!m_notificationsPinned)
        setNotificationsOpened(false);
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
    m_notificationsPinButton->setPos(headerGeometry.topLeft());
    if (isNotificationsOpened())
        setNotificationsOpened(); //there is no check there but it will fix the X-coord animation

    updateViewportMargins();
}

void QnWorkbenchUi::at_calendarShowButton_toggled(bool checked) {
    if(!m_ignoreClickEvent)
        setCalendarOpened(checked);
}

void QnWorkbenchUi::at_calendarItem_paintGeometryChanged() {
    if(m_inCalendarGeometryUpdate)
        return;

    QnScopedValueRollback<bool> guard(&m_inCalendarGeometryUpdate, true);

    updateCalendarGeometry();

    QRectF paintGeometry = m_calendarItem->geometry();

    /* Don't hide calendar item here. It will repaint itself when shown, which will
     * degrade performance. */

    //m_calendarBackgroundItem->setGeometry(paintGeometry);
    m_calendarShowButton->setPos(QPointF(
        paintGeometry.right() - m_calendarShowButton->size().height(),
        qMin(m_sliderItem->y(), paintGeometry.top())
    ));

    updateNotificationsGeometry();
}

void QnWorkbenchUi::at_calendarHidingProcessor_hoverFocusLeft() {
    setCalendarOpened(false);
}

void QnWorkbenchUi::at_sliderZoomInButton_pressed() {
    m_sliderZoomingIn = true;
}

void QnWorkbenchUi::at_sliderZoomInButton_released() {
    m_sliderZoomingIn = false;
}

void QnWorkbenchUi::at_sliderZoomOutButton_pressed() {
    m_sliderZoomingOut = true;
}

void QnWorkbenchUi::at_sliderZoomOutButton_released() {
    m_sliderZoomingOut = false;
}
