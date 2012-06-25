#include "workbench_ui.h"
#include <cassert>
#include <cmath> /* For std::floor. */

#include <QtCore/QSettings>
#include <QtCore/QTimer>

#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsProxyWidget>
#include <QtGui/QGraphicsLinearLayout>
#include <QtGui/QStyle>
#include <QtGui/QApplication>
#include <QtGui/QMenu>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/checked_cast.h>

#include <core/dataprovider/abstract_streamdataprovider.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/layout_resource.h>

#include <camera/resource_display.h>

#include <ui/animation/viewport_animator.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/ui_elements_instrument.h>
#include <ui/graphics/instruments/animation_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/bounding_instrument.h>
#include <ui/graphics/instruments/activity_listener_instrument.h>
#include <ui/graphics/instruments/fps_counting_instrument.h>
#include <ui/graphics/instruments/drop_instrument.h>
#include <ui/graphics/instruments/focus_listener_instrument.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/image_button_widget.h>
#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/items/masked_proxy_widget.h>
#include <ui/graphics/items/clickable_widget.h>
#include <ui/graphics/items/simple_frame_widget.h>
#include <ui/graphics/items/standard/graphics_label.h>
#include <ui/graphics/items/controls/navigation_item.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/controls/tool_tip_item.h>

#include <ui/processors/hover_processor.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/widgets/resource_tree_widget.h>
#include <ui/widgets/layout_tab_bar.h>
#include <ui/widgets/help_widget.h>
#include <ui/style/skin.h>

#include <help/qncontext_help.h>

#include "camera/camera.h"
#include "openal/qtvaudiodevice.h"
#include "core/resourcemanagment/resource_pool.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"

#include "extensions/workbench_render_watcher.h"
#include "extensions/workbench_motion_display_watcher.h"
#include "workbench.h"
#include "workbench_display.h"
#include "workbench_layout.h"
#include "workbench_context.h"
#include "workbench_navigator.h"


Q_DECLARE_METATYPE(VariantAnimator *)

namespace {

    QnImageButtonWidget *newActionButton(QAction *action, qreal sizeMultiplier = 1.0, QGraphicsItem *parent = NULL) {
        int baseSize = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, NULL, NULL);

        qreal height = baseSize * sizeMultiplier;
        qreal width = height * QnGeometry::aspectRatio(action->icon().actualSize(QSize(1024, 1024)));

        QnZoomingImageButtonWidget *button = new QnZoomingImageButtonWidget(parent);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::ToolButton);
        button->setMaximumSize(width, height);
        button->setMinimumSize(width, height);
        button->setDefaultAction(action);
        button->setCached(true);

        return button;
    }

    QnImageButtonWidget *newShowHideButton(QGraphicsItem *parent = NULL) {
        QnImageButtonWidget *button = new QnImageButtonWidget(parent);
        button->resize(15, 45);
        button->setIcon(qnSkin->icon("slide_right.png", "slide_left.png"));
        button->setCheckable(true);
        return button;
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

    class QnEventEatingWidget: public GraphicsWidget {
        typedef GraphicsWidget base_type;
    
    public:
        QnEventEatingWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags wFlags = 0):
            base_type(parent, wFlags)
        {
            setAcceptedMouseButtons(Qt::LeftButton);
            setAcceptHoverEvents(true);
            setFlag(ItemHasNoContents, true);
        }

    protected:
        virtual bool sceneEvent(QEvent *event) override {
            switch(event->type()) {
            case QEvent::GraphicsSceneMousePress:
            case QEvent::GraphicsSceneMouseRelease:
            case QEvent::GraphicsSceneMouseMove:
            case QEvent::GraphicsSceneMouseDoubleClick:
                event->accept();
                return true;
            default:
                return base_type::sceneEvent(event);
            }
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

    const qreal normalHelpOpacity = 0.85;
    const qreal hoverHelpOpacity = 0.95;
    const qreal normalHelpBackgroundOpacity = 0.5;
    const qreal hoverHelpBackgroundOpacity = 1.0;

    const int hideConstrolsTimeoutMSec = 2000;
    const int closeConstrolsTimeoutMSec = 2000;

} // anonymous namespace


QnWorkbenchUi::QnWorkbenchUi(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_instrumentManager(display()->instrumentManager()),
    m_treePinned(false),
    m_inactive(false),
    m_titleUsed(false),
    m_titleOpened(false),
    m_treeOpened(false),
    m_sliderOpened(false),
    m_titleVisible(false),
    m_treeVisible(false),
    m_sliderVisible(false),
    m_helpPinned(false),
    m_helpVisible(false),
    m_helpOpened(false),
    m_flags(0),
    m_ignoreClickEvent(false),
    m_inFreespace(false),
    m_ignoreSliderResizerGeometryChanges(false),
    m_ignoreSliderResizerGeometryChanges2(false),
    m_lastThumbnailsHeight(48.0)
{
    memset(m_widgetByRole, 0, sizeof(m_widgetByRole));

    /* Install and configure instruments. */
    m_fpsCountingInstrument = new FpsCountingInstrument(333, this);
    m_uiElementsInstrument = new UiElementsInstrument(this);
    m_controlsActivityInstrument = new ActivityListenerInstrument(true, hideConstrolsTimeoutMSec, this);

    m_instrumentManager->installInstrument(m_uiElementsInstrument, InstallationMode::InstallBefore, display()->paintForwardingInstrument());
    m_instrumentManager->installInstrument(m_fpsCountingInstrument, InstallationMode::InstallBefore, display()->paintForwardingInstrument());
    m_instrumentManager->installInstrument(m_controlsActivityInstrument);

    connect(m_controlsActivityInstrument, SIGNAL(activityStopped()),                                                                this,                           SLOT(at_activityStopped()));
    connect(m_controlsActivityInstrument, SIGNAL(activityResumed()),                                                                this,                           SLOT(at_activityStarted()));
    connect(m_fpsCountingInstrument,    SIGNAL(fpsChanged(qreal)),                                                                  this,                           SLOT(at_fpsChanged(qreal)));

    /* Create controls. */
    m_controlsWidget = m_uiElementsInstrument->widget(); /* Setting an ItemIsPanel flag on this item prevents focusing on graphics widgets. Don't set it. */
    display()->setLayer(m_controlsWidget, Qn::UiLayer);

    QnSingleEventSignalizer *deactivationSignalizer = new QnSingleEventSignalizer(this);
    deactivationSignalizer->setEventType(QEvent::WindowDeactivate);
    m_controlsWidget->installEventFilter(deactivationSignalizer);

    connect(deactivationSignalizer,     SIGNAL(activated(QObject *, QEvent *)),                                                     this,                           SLOT(at_controlsWidget_deactivated()));
    connect(m_controlsWidget,           SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_controlsWidget_geometryChanged()));


    /* Fps counter. */
    m_fpsItem = new GraphicsLabel(m_controlsWidget);
    m_fpsItem->setAcceptedMouseButtons(0);
    m_fpsItem->setAcceptsHoverEvents(false);
    m_fpsItem->setFont(QFont("Courier New", 10));
    {
        QPalette palette = m_fpsItem->palette();
        palette.setColor(QPalette::Window, QColor(0, 0, 0, 0));
        palette.setColor(QPalette::WindowText, QColor(63, 159, 216));
        m_fpsItem->setPalette(palette);
    }

    display()->view()->addAction(action(Qn::ShowFpsAction));
    connect(action(Qn::ShowFpsAction),  SIGNAL(toggled(bool)),                                                                      this,                           SLOT(setFpsVisible(bool)));
    connect(m_fpsItem,                  SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_fpsItem_geometryChanged()));
    setFpsVisible(false);


    /* Tree widget. */
    m_treeWidget = new QnResourceTreeWidget(NULL, context());
    m_treeWidget->setAttribute(Qt::WA_TranslucentBackground);
    {
        QPalette palette = m_treeWidget->palette();
        palette.setColor(QPalette::Window, Qt::transparent);
        palette.setColor(QPalette::Base, Qt::transparent);
        
        QPalette cbPalette = m_treeWidget->comboBoxPalette();
        cbPalette.setColor(QPalette::Window, Qt::black);
        cbPalette.setColor(QPalette::Base, Qt::black);
        
        m_treeWidget->setPalette(palette);
        m_treeWidget->setComboBoxPalette(cbPalette);
    }
    m_treeWidget->resize(250, 0);

    m_treeBackgroundItem = new QnSimpleFrameWidget(m_controlsWidget);
    m_treeBackgroundItem->setAutoFillBackground(true);
    {
        QPalette palette = m_treeBackgroundItem->palette();

        QLinearGradient gradient(0, 0, 1, 0);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(0.0, QColor(0, 0, 0, 255));
        gradient.setColorAt(1.0, QColor(0, 0, 0, 64));
        gradient.setSpread(QGradient::RepeatSpread);

        palette.setBrush(QPalette::Window, QBrush(gradient));
        m_treeBackgroundItem->setPalette(palette);
    }
    m_treeBackgroundItem->setFrameColor(QColor(110, 110, 110, 255));
    m_treeBackgroundItem->setFrameWidth(0.5);

    m_treeItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_treeItem->setWidget(m_treeWidget);
    m_treeItem->setFocusPolicy(Qt::StrongFocus);

    m_treePinButton = new QnImageButtonWidget(m_controlsWidget);
    m_treePinButton->resize(24, 24);
    m_treePinButton->setIcon(qnSkin->icon("decorations/pin.png", "decorations/unpin.png"));
    m_treePinButton->setCheckable(true);
    m_treePinButton->setFocusProxy(m_treeItem);

    m_treeShowButton = newShowHideButton(m_controlsWidget);
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
    connect(m_treePinButton,            SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_treePinButton_toggled(bool)));
    connect(m_treeShowButton,           SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_treeShowButton_toggled(bool)));
    connect(m_treeOpacityProcessor,     SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateTreeOpacity()));
    connect(m_treeOpacityProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateTreeOpacity()));
    connect(m_treeOpacityProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateControlsVisibility()));
    connect(m_treeOpacityProcessor,     SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateControlsVisibility()));
    connect(m_treeHidingProcessor,      SIGNAL(hoverFocusLeft()),                                                                   this,                           SLOT(at_treeHidingProcessor_hoverFocusLeft()));
    connect(m_treeShowingProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(at_treeShowingProcessor_hoverEntered()));
    connect(m_treeItem,                 SIGNAL(paintRectChanged()),                                                                 this,                           SLOT(at_treeItem_paintGeometryChanged()));
    connect(m_treeItem,                 SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_treeItem_paintGeometryChanged()));


    /* Title bar. */
    m_titleBackgroundItem = new QnSimpleFrameWidget(m_controlsWidget);
    m_titleBackgroundItem->setAutoFillBackground(true);
    {
        QPalette palette = m_titleBackgroundItem->palette();

        QLinearGradient gradient(0, 0, 0, 1);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(0.0, QColor(0, 0, 0, 255));
        gradient.setColorAt(1.0, QColor(0, 0, 0, 64));
        gradient.setSpread(QGradient::RepeatSpread);

        palette.setBrush(QPalette::Window, QBrush(gradient));
        m_titleBackgroundItem->setPalette(palette);
    }
    m_titleBackgroundItem->setFrameColor(QColor(110, 110, 110, 255));
    m_titleBackgroundItem->setFrameWidth(0.5);

    m_titleItem = new QnClickableWidget(m_controlsWidget);
    m_titleItem->setPos(0.0, 0.0);
    m_titleItem->setClickableButtons(Qt::LeftButton);

    QnSingleEventSignalizer *titleMenuSignalizer = new QnSingleEventSignalizer(this);
    titleMenuSignalizer->setEventType(QEvent::GraphicsSceneContextMenu);
    m_titleItem->installEventFilter(titleMenuSignalizer);

    m_tabBarItem = new QGraphicsProxyWidget(m_controlsWidget);
    m_tabBarItem->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    
    m_tabBarWidget = new QnLayoutTabBar(NULL, context());
    m_tabBarWidget->setAttribute(Qt::WA_TranslucentBackground);
    m_tabBarItem->setWidget(m_tabBarWidget);

    m_mainMenuButton = newActionButton(action(Qn::LightMainMenuAction), 1.5);

    QGraphicsLinearLayout *titleLayout = new QGraphicsLinearLayout();

    titleLayout->setSpacing(2);
    titleLayout->setContentsMargins(4, 0, 4, 0);
    QGraphicsLinearLayout *titleLeftButtonsLayout = new QGraphicsLinearLayout();
    titleLeftButtonsLayout->setSpacing(2);
    titleLeftButtonsLayout->setContentsMargins(0, 0, 0, 0);
    titleLeftButtonsLayout->addItem(m_mainMenuButton);
    titleLayout->addItem(titleLeftButtonsLayout);
    titleLayout->addItem(m_tabBarItem);
    titleLayout->setAlignment(m_tabBarItem, Qt::AlignBottom);
    QGraphicsLinearLayout *titleRightButtonsLayout = new QGraphicsLinearLayout();
    titleRightButtonsLayout->setSpacing(2);
    titleRightButtonsLayout->setContentsMargins(0, 4, 0, 0);
    titleRightButtonsLayout->addItem(newActionButton(action(Qn::OpenNewTabAction)));
    titleRightButtonsLayout->addStretch(0x1000);
    titleRightButtonsLayout->addItem(newActionButton(action(Qn::ConnectToServerAction)));
    QGraphicsWidget *titleSpacerWidget = new QGraphicsWidget();
    titleSpacerWidget->setMinimumSize(QSizeF(6.0, 6.0));
    titleSpacerWidget->setMaximumSize(titleSpacerWidget->minimumSize());
    titleRightButtonsLayout->addItem(titleSpacerWidget);
    titleRightButtonsLayout->addItem(newActionButton(action(Qn::MinimizeAction)));
    titleRightButtonsLayout->addItem(newActionButton(action(Qn::FullscreenAction)));
    titleRightButtonsLayout->addItem(newActionButton(action(Qn::ExitAction)));
    titleLayout->addItem(titleRightButtonsLayout);
    m_titleItem->setLayout(titleLayout);
    titleLayout->activate(); /* So that it would set title's size. */

    m_titleShowButton = newShowHideButton(m_controlsWidget);
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
    m_titleYAnimator->setSpeed(m_titleItem->size().height() * 2.0);
    m_titleYAnimator->setTimeLimit(500);

    m_titleOpacityAnimatorGroup = new AnimatorGroup(this);
    m_titleOpacityAnimatorGroup->setTimer(m_instrumentManager->animationTimer());
    m_titleOpacityAnimatorGroup->addAnimator(opacityAnimator(m_titleItem));
    m_titleOpacityAnimatorGroup->addAnimator(opacityAnimator(m_titleBackgroundItem)); /* Speed of 1.0 is OK here. */
    m_titleOpacityAnimatorGroup->addAnimator(opacityAnimator(m_titleShowButton));

    connect(m_tabBarWidget,             SIGNAL(closeRequested(QnWorkbenchLayout *)),                                                this,                           SLOT(at_tabBar_closeRequested(QnWorkbenchLayout *)));
    connect(m_titleShowButton,          SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_titleShowButton_toggled(bool)));
    connect(m_titleOpacityProcessor,    SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateTitleOpacity()));
    connect(m_titleOpacityProcessor,    SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateTitleOpacity()));
    connect(m_titleOpacityProcessor,    SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateControlsVisibility()));
    connect(m_titleOpacityProcessor,    SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateControlsVisibility()));
    connect(m_titleItem,                SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_titleItem_geometryChanged()));
    connect(m_titleItem,                SIGNAL(doubleClicked()),                                                                    action(Qn::FullscreenAction),   SLOT(toggle()));
    connect(titleMenuSignalizer,        SIGNAL(activated(QObject *, QEvent *)),                                                     this,                           SLOT(at_titleItem_contextMenuRequested(QObject *, QEvent *)));
    connect(action(Qn::MainMenuAction), SIGNAL(triggered()),                                                                        this,                           SLOT(at_mainMenuAction_triggered()));


    /* Help window. */
    m_helpBackgroundItem = new QnSimpleFrameWidget(m_controlsWidget);
    m_helpBackgroundItem->setAutoFillBackground(true);
    {
        QPalette palette = m_helpBackgroundItem->palette();

        QLinearGradient gradient(0, 0, 1, 0);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(1.0, QColor(0, 0, 0, 255));
        gradient.setColorAt(0.0, QColor(0, 0, 0, 64));
        gradient.setSpread(QGradient::RepeatSpread);

        palette.setBrush(QPalette::Window, QBrush(gradient));
        m_helpBackgroundItem->setPalette(palette);
    }
    m_helpBackgroundItem->setFrameColor(QColor(110, 110, 110, 255));
    m_helpBackgroundItem->setFrameWidth(0.5);

    m_helpWidget = new QnHelpWidget(qnHelp);
    m_helpWidget->setAttribute(Qt::WA_TranslucentBackground);
    {
        QPalette palette = m_helpWidget->palette();
        palette.setColor(QPalette::Window, Qt::transparent);
        palette.setColor(QPalette::Base, Qt::transparent);
        m_helpWidget->setPalette(palette);
    }
    m_helpWidget->resize(250, 0);


    m_helpItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_helpItem->setWidget(m_helpWidget);

    m_helpPinButton = new QnImageButtonWidget(m_controlsWidget);
    m_helpPinButton->resize(24, 24);
    m_helpPinButton->setIcon(qnSkin->icon("decorations/pin.png", "decorations/unpin.png"));
    m_helpPinButton->setCheckable(true);
    m_helpPinButton->setFocusProxy(m_helpItem);

    m_helpShowButton = newShowHideButton(m_controlsWidget);
    m_helpShowButton->setTransform(QTransform::fromScale(-1, 1));

    m_helpShowButton->setFocusProxy(m_helpItem);

    m_helpOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_helpOpacityProcessor->addTargetItem(m_helpItem);
    m_helpOpacityProcessor->addTargetItem(m_helpShowButton);

    m_helpHidingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_helpHidingProcessor->addTargetItem(m_helpItem);
    m_helpHidingProcessor->addTargetItem(m_helpShowButton);
    m_helpHidingProcessor->setHoverLeaveDelay(closeConstrolsTimeoutMSec);
    m_helpHidingProcessor->setFocusLeaveDelay(closeConstrolsTimeoutMSec);

    m_helpShowingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_helpShowingProcessor->addTargetItem(m_helpShowButton);
    m_helpShowingProcessor->setHoverEnterDelay(250);

    m_helpXAnimator = new VariantAnimator(this);
    m_helpXAnimator->setTimer(m_instrumentManager->animationTimer());
    m_helpXAnimator->setTargetObject(m_helpItem);
    m_helpXAnimator->setAccessor(new PropertyAccessor("x"));
    m_helpXAnimator->setSpeed(m_helpItem->size().width() * 2.0);
    m_helpXAnimator->setTimeLimit(500);

    m_helpOpacityAnimatorGroup = new AnimatorGroup(this);
    m_helpOpacityAnimatorGroup->setTimer(m_instrumentManager->animationTimer());
    m_helpOpacityAnimatorGroup->addAnimator(opacityAnimator(m_helpItem));
    m_helpOpacityAnimatorGroup->addAnimator(opacityAnimator(m_helpBackgroundItem)); /* Speed of 1.0 is OK here. */
    m_helpOpacityAnimatorGroup->addAnimator(opacityAnimator(m_helpShowButton));
    m_helpOpacityAnimatorGroup->addAnimator(opacityAnimator(m_helpPinButton));

    connect(m_helpPinButton,            SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_helpPinButton_toggled(bool)));
    connect(m_helpShowButton,           SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_helpShowButton_toggled(bool)));
    connect(m_helpOpacityProcessor,     SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateHelpOpacity()));
    connect(m_helpOpacityProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateHelpOpacity()));
    connect(m_helpOpacityProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateControlsVisibility()));
    connect(m_helpOpacityProcessor,     SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateControlsVisibility()));
    connect(m_helpHidingProcessor,      SIGNAL(hoverFocusLeft()),                                                                   this,                           SLOT(at_helpHidingProcessor_hoverFocusLeft()));
    connect(m_helpShowingProcessor,     SIGNAL(hoverEntered()),                                                                     this,                           SLOT(at_helpShowingProcessor_hoverEntered()));
    connect(m_helpItem,                 SIGNAL(paintRectChanged()),                                                                 this,                           SLOT(at_helpItem_paintGeometryChanged()));
    connect(m_helpItem,                 SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_helpItem_paintGeometryChanged()));


    /* Navigation slider. */
    m_sliderEaterItem = new QnEventEatingWidget(m_controlsWidget);

    m_sliderResizerItem = new QnTopResizerWidget(m_controlsWidget);
    m_sliderResizerItem->setProperty(Qn::NoHandScrollOver, true);
    m_instrumentManager->registerItem(m_sliderResizerItem); /* We want it registered right away. */

    m_sliderItem = new QnNavigationItem(m_controlsWidget);
    m_sliderItem->setFrameColor(QColor(110, 110, 110, 255));
    m_sliderItem->setFrameWidth(0.5);
    m_sliderItem->timeSlider()->toolTipItem()->setParentItem(m_controlsWidget);

    m_sliderShowButton = newShowHideButton(m_controlsWidget);
    {
        QTransform transform;
        transform.rotate(-90);
        m_sliderShowButton->setTransform(transform);
    }
    m_sliderShowButton->setFocusProxy(m_sliderItem);

    /* There is no stackAfter function, so we have to resort to ugly copypasta. */
    m_sliderShowButton->stackBefore(m_sliderItem->timeSlider()->toolTipItem());
    m_sliderResizerItem->stackBefore(m_sliderShowButton);
    m_sliderItem->stackBefore(m_sliderResizerItem);

    m_sliderOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_sliderOpacityProcessor->addTargetItem(m_sliderItem);
    m_sliderOpacityProcessor->addTargetItem(m_sliderItem->timeSlider()->toolTipItem());
    m_sliderOpacityProcessor->addTargetItem(m_sliderShowButton);
    m_sliderOpacityProcessor->addTargetItem(m_sliderResizerItem);

    m_sliderYAnimator = new VariantAnimator(this);
    m_sliderYAnimator->setTimer(m_instrumentManager->animationTimer());
    m_sliderYAnimator->setTargetObject(m_sliderItem);
    m_sliderYAnimator->setAccessor(new PropertyAccessor("y"));
    m_sliderYAnimator->setSpeed(m_sliderItem->size().height() * 2.0);
    m_sliderYAnimator->setTimeLimit(500);

    m_sliderOpacityAnimatorGroup = new AnimatorGroup(this);
    m_sliderOpacityAnimatorGroup->setTimer(m_instrumentManager->animationTimer());
    m_sliderOpacityAnimatorGroup->addAnimator(opacityAnimator(m_sliderItem));
    m_sliderOpacityAnimatorGroup->addAnimator(opacityAnimator(m_sliderShowButton)); /* Speed of 1.0 is OK here. */

    connect(m_sliderShowButton,         SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_sliderShowButton_toggled(bool)));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateSliderOpacity()));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateSliderOpacity()));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateControlsVisibility()));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateControlsVisibility()));
    connect(m_sliderItem,               SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_sliderItem_geometryChanged()));
    connect(m_sliderResizerItem,        SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_sliderResizerItem_geometryChanged()));
    connect(navigator(),                SIGNAL(currentWidgetChanged()),                                                             this,                           SLOT(updateControlsVisibility()));
    connect(action(Qn::ToggleThumbnailsAction), SIGNAL(toggled(bool)),                                                              this,                           SLOT(at_toggleThumbnailsAction_toggled(bool)));


    /* Connect to display. */
    display()->view()->addAction(action(Qn::FreespaceAction));
    connect(action(Qn::FreespaceAction),SIGNAL(triggered()),                                                                        this,                           SLOT(at_freespaceAction_triggered()));
    connect(action(Qn::FullscreenAction),SIGNAL(triggered()),                                                                       this,                           SLOT(at_fullscreenAction_triggered()));
    connect(display(),                  SIGNAL(viewportGrabbed()),                                                                  this,                           SLOT(disableProxyUpdates()));
    connect(display(),                  SIGNAL(viewportUngrabbed()),                                                                this,                           SLOT(enableProxyUpdates()));
    connect(display(),                  SIGNAL(widgetChanged(Qn::ItemRole)),                                                        this,                           SLOT(at_display_widgetChanged(Qn::ItemRole)));


    /* Init fields. */
    setFlags(HideWhenNormal | HideWhenZoomed | AdjustMargins);

    setSliderOpened(true, false);
    setSliderVisible(false, false);
    setTreeOpened(true, false);
    setTreeVisible(true, false);
    setTitleOpened(true, false);
    setTitleVisible(true, false);
    setTitleUsed(false);
    setHelpOpened(false, false);
    setHelpVisible(true, false);

    /* Tree is pinned by default. */
    m_treePinButton->setChecked(true);

    /* Set up title D&D. */
    DropInstrument *dropInstrument = new DropInstrument(true, context(), this);
    display()->instrumentManager()->installInstrument(dropInstrument);
    dropInstrument->setSurface(m_titleBackgroundItem);

    /* Set up help context processing. */
    m_motionDisplayWatcher = new QnWorkbenchMotionDisplayWatcher(display(), this);
    connect(display()->focusListenerInstrument(), SIGNAL(focusItemChanged()),                                                       this,                           SLOT(updateHelpContext()));
    connect(m_treeWidget,               SIGNAL(currentTabChanged()),                                                                this,                           SLOT(updateHelpContext()));
    connect(m_motionDisplayWatcher,     SIGNAL(motionGridShown()),                                                                  this,                           SLOT(updateHelpContext()));
    connect(m_motionDisplayWatcher,     SIGNAL(motionGridHidden()),                                                                 this,                           SLOT(updateHelpContext()));
    connect(m_helpWidget,               SIGNAL(showRequested()),                                                                    this,                           SLOT(at_helpWidget_showRequested()));
    connect(m_helpWidget,               SIGNAL(hideRequested()),                                                                    this,                           SLOT(at_helpWidget_hideRequested()));
    updateHelpContext();
}

QnWorkbenchUi::~QnWorkbenchUi() {
    return;
}

Qn::ActionScope QnWorkbenchUi::currentScope() const {
    QGraphicsItem *focusItem = display()->scene()->focusItem();
    if(focusItem == m_treeItem) {
        return Qn::TreeScope;
    } else if(focusItem == m_sliderItem) {
        return Qn::SliderScope;
    } else if(focusItem == m_tabBarItem) {
        return Qn::TabBarScope;
    } else if(!focusItem || dynamic_cast<QnResourceWidget *>(focusItem)) {
        return Qn::SceneScope;
    } else {
        return Qn::MainScope;
    }
}

QVariant QnWorkbenchUi::currentTarget(Qn::ActionScope scope) const {
    /* Get items. */
    switch(scope) {
    case Qn::TabBarScope: 
        return m_tabBarWidget->currentTarget(scope);
    case Qn::TreeScope:
        return m_treeWidget->currentTarget(scope);
    case Qn::SliderScope:
        return QVariant::fromValue(navigator()->currentWidget());
    case Qn::SceneScope:
        return QVariant::fromValue(QnActionParameterTypes::widgets(display()->scene()->selectedItems()));
    default:
        return QVariant();
    }
}

void QnWorkbenchUi::setTreeOpened(bool opened, bool animate) {
    m_inFreespace = false;

    m_treeShowingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    m_treeOpened = opened;

    qreal newX = opened ? 0.0 : -m_treeItem->size().width() - 1.0 /* Just in case. */;
    if (animate) {
        m_treeXAnimator->animateTo(newX);
    } else {
        m_treeXAnimator->stop();
        m_treeItem->setX(newX);
    }

    QnScopedValueRollback<bool> rollback(&m_ignoreClickEvent, true);
    m_treeShowButton->setChecked(opened);
}

void QnWorkbenchUi::setSliderOpened(bool opened, bool animate) {
    m_inFreespace = false;

    m_sliderOpened = opened;

    qreal newY = m_controlsWidgetRect.bottom() + (opened ? -m_sliderItem->size().height() : 48.0 /* So that tooltips are not opened. */);
    if (animate) {
        m_sliderYAnimator->animateTo(newY);
    } else {
        m_sliderYAnimator->stop();
        m_sliderItem->setY(newY);
    }

    QnScopedValueRollback<bool> rollback(&m_ignoreClickEvent, true);
    m_sliderShowButton->setChecked(opened);
}

void QnWorkbenchUi::setTitleOpened(bool opened, bool animate) {
    m_inFreespace = false;

    m_titleOpened = opened;

    if(!m_titleUsed)
        return;

    qreal newY = opened ? 0.0 : -m_titleItem->size().height() - 1.0;
    if (animate) {
        m_titleYAnimator->animateTo(newY);
    } else {
        m_titleYAnimator->stop();
        m_titleItem->setY(newY);
    }

    QnScopedValueRollback<bool> rollback(&m_ignoreClickEvent, true);
    m_titleShowButton->setChecked(opened);
}

void QnWorkbenchUi::setHelpOpened(bool opened, bool animate) {
    m_inFreespace = false;

    m_helpShowingProcessor->forceHoverLeave(); /* So that it don't bring it back. */

    m_helpOpened = opened;

    qreal newX = m_controlsWidgetRect.right() + (opened ? -m_helpItem->size().width() : 1.0 /* Just in case. */);
    if (animate) {
        m_helpXAnimator->animateTo(newX);
    } else {
        m_helpXAnimator->stop();
        m_helpItem->setX(newX);
    }

    QnScopedValueRollback<bool> rollback(&m_ignoreClickEvent, true);
    m_helpShowButton->setChecked(opened);
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
        updateHelpGeometry();
    }
}

void QnWorkbenchUi::setTitleVisible(bool visible, bool animate) {
    bool changed = m_titleVisible != visible;

    m_titleVisible = visible;

    updateTitleOpacity(animate);
    if(changed) {
        updateTreeGeometry();
        updateHelpGeometry();
    }
}

void QnWorkbenchUi::setHelpVisible(bool visible, bool animate) {
    bool changed = m_helpVisible != visible;

    m_helpVisible = visible;

    updateHelpOpacity(animate);
    if(changed)
        updateHelpGeometry();
}


void QnWorkbenchUi::setProxyUpdatesEnabled(bool updatesEnabled) {
    m_helpItem->setUpdatesEnabled(updatesEnabled);
    m_treeItem->setUpdatesEnabled(updatesEnabled);
}

void QnWorkbenchUi::setTitleUsed(bool used) {
    m_titleItem->setVisible(used);
    m_titleBackgroundItem->setVisible(used);
    m_titleShowButton->setVisible(used);

    if(used) {
        m_titleUsed = used;

        setTitleOpened(m_titleOpened, false);

        at_titleItem_geometryChanged();

        /* For reasons unknown, tab bar's size gets messed up when it is shown 
         * after new items were added to it. Re-embedding helps, probably there is
         * a simpler workaround. */
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
    m_sliderEaterItem->setVisible(!qFuzzyIsNull(opacity));
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

void QnWorkbenchUi::setHelpOpacity(qreal foregroundOpacity, qreal backgroundOpacity, bool animate) {
    if(animate) {
        m_helpOpacityAnimatorGroup->pause();
        opacityAnimator(m_helpItem)->setTargetValue(foregroundOpacity);
        opacityAnimator(m_helpPinButton)->setTargetValue(foregroundOpacity);
        opacityAnimator(m_helpBackgroundItem)->setTargetValue(backgroundOpacity);
        opacityAnimator(m_helpShowButton)->setTargetValue(backgroundOpacity);
        m_helpOpacityAnimatorGroup->start();
    } else {
        m_helpOpacityAnimatorGroup->stop();
        m_helpItem->setOpacity(foregroundOpacity);
        m_helpPinButton->setOpacity(foregroundOpacity);
        m_helpBackgroundItem->setOpacity(backgroundOpacity);
        m_helpShowButton->setOpacity(backgroundOpacity);
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
    } else {
        if(m_sliderOpacityProcessor->isHovered()) {
            setSliderOpacity(hoverSliderOpacity, animate);
        } else {
            setSliderOpacity(normalSliderOpacity, animate);
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

void QnWorkbenchUi::updateHelpOpacity(bool animate) {
    if(!m_helpVisible) {
        setHelpOpacity(0.0, 0.0, animate);
    } else {
        if(m_helpOpacityProcessor->isHovered()) {
            setHelpOpacity(hoverHelpOpacity, hoverHelpBackgroundOpacity, animate);
        } else {
            setHelpOpacity(normalHelpOpacity, normalHelpBackgroundOpacity, animate);
        }
    }
}

void QnWorkbenchUi::updateControlsVisibility(bool animate) {
    bool sliderVisible = navigator()->currentWidget() != NULL && !navigator()->currentWidget()->display()->isStillImage() && !(navigator()->currentWidget()->resource()->flags() & QnResource::still_image);

    if(m_inactive) {
        bool hovered = m_sliderOpacityProcessor->isHovered() || m_treeOpacityProcessor->isHovered() || m_titleOpacityProcessor->isHovered() || m_helpOpacityProcessor->isHovered();
        setSliderVisible(sliderVisible && hovered, animate);
        setTreeVisible(hovered, animate);
        setTitleVisible(hovered, animate);
        setHelpVisible(hovered, animate);
    } else {
        setSliderVisible(sliderVisible, animate);
        setTreeVisible(true, animate);
        setTitleVisible(true, animate);
        setHelpVisible(true, animate);
    }
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

QRectF QnWorkbenchUi::updatedHelpGeometry(const QRectF &helpGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry) {
    QPointF pos(
        helpGeometry.x(),
        ((!m_titleVisible || !m_titleUsed) && m_helpVisible) ? 30.0 : qMax(titleGeometry.bottom() + 30.0, 30.0)
    );
    QSizeF size(
        helpGeometry.width(),
        ((!m_sliderVisible && m_helpVisible) ? m_controlsWidgetRect.bottom() - 30.0 : qMin(sliderGeometry.y() - 30.0, m_controlsWidgetRect.bottom() - 30.0)) - pos.y()
    );
    return QRectF(pos, size);
}

void QnWorkbenchUi::updateHelpGeometry() {
    /* Update painting rect the "fair" way. */
    QRectF geometry = updatedHelpGeometry(m_helpItem->geometry(), m_titleItem->geometry(), m_sliderItem->geometry());
    m_helpItem->setPaintRect(QRectF(QPointF(0.0, 0.0), geometry.size()));

    /* Always change position. */
    m_helpItem->setPos(geometry.topLeft());

    /* Whether actual size change should be deferred. */
    bool defer = false;

    /* Calculate slider target position. */
    QPointF sliderPos;
    if(!m_sliderVisible && m_helpVisible) {
        sliderPos = QPointF(m_sliderItem->pos().x(), m_controlsWidgetRect.bottom());
    } else if(m_sliderYAnimator->isRunning()) {
        sliderPos = QPointF(m_sliderItem->pos().x(), m_sliderYAnimator->targetValue().toReal());
        defer |= !qFuzzyCompare(sliderPos, m_sliderItem->pos()); /* If animation is running, then geometry sync should be deferred. */
    } else {
        sliderPos = m_sliderItem->pos();
    }

    /* Calculate title target position. */
    QPointF titlePos;
    if((!m_titleVisible || !m_titleUsed) && m_helpVisible) {
        titlePos = QPointF(m_titleItem->pos().x(), -m_titleItem->size().height());
    } else if(m_titleYAnimator->isRunning()) {
        titlePos = QPointF(m_titleItem->pos().x(), m_titleYAnimator->targetValue().toReal());
        defer |= !qFuzzyCompare(titlePos, m_titleItem->pos());
    } else {
        titlePos = m_titleItem->pos();
    }

    /* Calculate target geometry. */
    geometry = updatedHelpGeometry(m_helpItem->geometry(), QRectF(titlePos, m_titleItem->size()), QRectF(sliderPos, m_sliderItem->size()));
    if(qFuzzyCompare(geometry, m_helpItem->geometry()))
        return;

    /* Defer size change if it doesn't cause empty space to occur. */
    if(defer && geometry.height() < m_helpItem->size().height())
        return;

    m_helpItem->resize(geometry.size());
}

void QnWorkbenchUi::updateFpsGeometry() {
    QPointF pos = QPointF(
        m_controlsWidgetRect.right() - m_fpsItem->size().width(),
        m_titleUsed ? m_titleItem->geometry().bottom() : 0.0
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
        m_sliderResizerItem->setPos(sliderResizerGeometry.topLeft());  // TODO: remove this ugly hack.
    }
}

QMargins QnWorkbenchUi::calculateViewportMargins(qreal treeX, qreal treeW, qreal titleY, qreal titleH, qreal sliderY, qreal helpX) {
    return QMargins(
        m_treePinned ? std::floor(qMax(0.0, treeX + treeW)) : 0.0,
        std::floor(qMax(0.0, titleY + titleH)),
        std::floor(qMax(0.0, m_helpPinned ? m_controlsWidgetRect.right() - helpX : 0.0)),
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

void QnWorkbenchUi::setTreeShowButtonUsed(bool used) {
    if(used) {
        m_treeShowButton->setAcceptedMouseButtons(Qt::LeftButton);
    } else {
        m_treeShowButton->setAcceptedMouseButtons(0);
    }
}

void QnWorkbenchUi::setHelpShowButtonUsed(bool used) {
    if(used) {
        m_helpShowButton->setAcceptedMouseButtons(Qt::LeftButton);
    } else {
        m_helpShowButton->setAcceptedMouseButtons(0);
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
            m_helpXAnimator->isRunning() ? m_helpXAnimator->targetValue().toReal() : m_helpItem->pos().x()
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
    Qn::ActionScope scope = Qn::InvalidScope;

    QGraphicsItem *focusItem = display()->scene()->focusItem();

    if(focusItem == NULL || dynamic_cast<QnResourceWidget *>(focusItem)) {
        scope = Qn::SceneScope;
    } else if(focusItem == m_helpItem || m_helpItem->isAncestorOf(focusItem) || focusItem == m_titleItem || m_titleItem->isAncestorOf(focusItem)) {
        return; /* Focusing on help widget or title item shouldn't change help context. */
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

    qnHelp->setHelpContext(context);
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

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchUi::at_freespaceAction_triggered() {
    bool isFullscreen = action(Qn::FullscreenAction)->isChecked();

    if(!m_inFreespace)
        m_inFreespace = isFullscreen && !isTreeOpened() && !isTitleOpened() && !isHelpOpened() && !isSliderOpened();

    if(!m_inFreespace) {
        if(!isFullscreen)
            action(Qn::FullscreenAction)->setChecked(true);
        
        setTreeOpened(false, isFullscreen);
        setTitleOpened(false, isFullscreen);
        setHelpOpened(false, isFullscreen);
        setSliderOpened(false, isFullscreen);

        updateViewportMargins(); /* This one is needed here so that fit-in-view operates on correct margins. */ // TODO: change code so that this call is not needed.
        action(Qn::FitInViewAction)->trigger();

        m_inFreespace = true;
    } else {
        setTreeOpened(true, isFullscreen);
        setTitleOpened(true, isFullscreen);
        setHelpOpened(false, isFullscreen);
        setSliderOpened(true, isFullscreen);

        m_inFreespace = false;
    }
}

void QnWorkbenchUi::at_fullscreenAction_triggered() {
    if(m_inFreespace)
        at_freespaceAction_triggered();
}

void QnWorkbenchUi::at_mainMenuAction_triggered() {
    if(!m_mainMenuButton->isVisible())
        return;

    const char *inShowMenuPropertyName = "_qn_inShowMenu";

    QMenu *menu = m_mainMenuButton->defaultAction()->menu();
    if(m_mainMenuButton->property(inShowMenuPropertyName).toBool()) {
        if(menu)
            menu->hide();
    } else {
        m_mainMenuButton->setProperty(inShowMenuPropertyName, true);
        m_mainMenuButton->click(); /* This call starts event loop. */
        m_mainMenuButton->setProperty(inShowMenuPropertyName, false);
    }
}


void QnWorkbenchUi::at_fpsChanged(qreal fps) {
    m_fpsItem->setText(QString::number(fps, 'g', 4));
    m_fpsItem->resize(m_fpsItem->effectiveSizeHint(Qt::PreferredSize));
}

void QnWorkbenchUi::at_activityStopped() {
    m_inactive = true;

    updateControlsVisibility(true);

    foreach(QnResourceWidget *widget, display()->widgets())
        widget->fadeOutOverlay();
}

void QnWorkbenchUi::at_activityStarted() {
    m_inactive = false;

    updateControlsVisibility(true);
}

void QnWorkbenchUi::at_display_widgetChanged(Qn::ItemRole role) {
    QnResourceWidget *oldWidget = m_widgetByRole[role];
    QnResourceWidget *newWidget = display()->widget(role);
    m_widgetByRole[role] = newWidget;

    /* Tune activity listener instrument. */
    if(role == Qn::ZoomedRole) {
        updateActivityInstrumentState();
        updateViewportMargins();
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

    m_helpItem->setGeometry(QRectF(
        m_helpItem->pos().x() - oldRect.width() + rect.width(),
        m_helpItem->pos().y(),
        m_helpItem->size().width(),
        m_helpItem->size().height() - oldRect.height() + rect.height()
    ));

    updateTreeGeometry();
    updateHelpGeometry();
    updateFpsGeometry();
}

void QnWorkbenchUi::at_sliderShowButton_toggled(bool checked) {
    if(!m_ignoreClickEvent)
        setSliderOpened(checked);
}

void QnWorkbenchUi::at_sliderItem_geometryChanged() {
    setSliderOpened(m_sliderOpened, m_sliderYAnimator->isRunning()); /* Re-adjust to screen sides. */

    updateTreeGeometry();
    updateHelpGeometry();

    updateViewportMargins();
    updateSliderResizerGeometry();

    m_sliderEaterItem->resize(m_sliderItem->size().width(), 30);
    m_sliderEaterItem->setPos(m_sliderItem->pos() + QPointF(0, -30));

    QRectF geometry = m_sliderItem->geometry();
    m_sliderShowButton->setPos(QPointF(
        (geometry.left() + geometry.right() - m_titleShowButton->size().height()) / 2,
        qMin(m_controlsWidgetRect.bottom(), geometry.top())
    ));
}

void QnWorkbenchUi::at_toggleThumbnailsAction_toggled(bool checked) {
    setThumbnailsVisible(checked);
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
    if(!m_treePinned)
        setTreeOpened(false);
}

void QnWorkbenchUi::at_treeShowingProcessor_hoverEntered() {
    if(!m_treePinned && !isTreeOpened()) {
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

void QnWorkbenchUi::at_treePinButton_toggled(bool checked) {
    m_treePinned = checked;

    if(checked)
        setTreeOpened(true);

    updateViewportMargins();
}

void QnWorkbenchUi::at_tabBar_closeRequested(QnWorkbenchLayout *layout) {
    QnWorkbenchLayoutList layouts;
    layouts.push_back(layout);
    menu()->trigger(Qn::CloseLayoutAction, layouts);
}

void QnWorkbenchUi::at_titleShowButton_toggled(bool checked) {
    if(!m_ignoreClickEvent)
        setTitleOpened(checked);
}

void QnWorkbenchUi::at_titleItem_geometryChanged() {
    if(!m_titleUsed)
        return;

    updateFpsGeometry();
    updateTreeGeometry();
    updateHelpGeometry();

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

void QnWorkbenchUi::at_helpPinButton_toggled(bool checked) {
    m_helpPinned = checked;

    if(checked)
        setHelpOpened(true);

    updateViewportMargins();
}

void QnWorkbenchUi::at_helpShowButton_toggled(bool checked) {
    if(!m_ignoreClickEvent)
        setHelpOpened(checked);
}

void QnWorkbenchUi::at_helpHidingProcessor_hoverFocusLeft() {
    if(!m_helpPinned)
        setHelpOpened(false);
}

void QnWorkbenchUi::at_helpShowingProcessor_hoverEntered() {
    if(!m_helpPinned && !isHelpOpened()) {
        setHelpOpened(true);

        /* So that the click that may follow won't hide it. */
        setHelpShowButtonUsed(false);
        QTimer::singleShot(300, this, SLOT(setHelpShowButtonUsed()));
    }

    m_helpHidingProcessor->forceHoverEnter();
    m_helpOpacityProcessor->forceHoverEnter();
}

void QnWorkbenchUi::at_helpItem_paintGeometryChanged() {
    QRectF paintGeometry = m_helpItem->paintGeometry();

    /* Don't hide help item here. It will repaint itself when shown, which will
     * degrade performance. */

    m_helpBackgroundItem->setGeometry(paintGeometry);
    m_helpShowButton->setPos(QPointF(
        qMin(m_controlsWidgetRect.right(), paintGeometry.left()),
        (paintGeometry.top() + paintGeometry.bottom() - m_helpShowButton->size().height()) / 2
    ));
    m_helpPinButton->setPos(paintGeometry.topLeft());

    updateViewportMargins();
}

void QnWorkbenchUi::at_helpWidget_showRequested() {
    m_helpHidingProcessor->forceHoverEnter();
    m_helpShowingProcessor->forceHoverEnter();

    setHelpOpened(true);
}

void QnWorkbenchUi::at_helpWidget_hideRequested() {
    m_helpHidingProcessor->forceHoverLeave();
    m_helpShowingProcessor->forceHoverLeave();

    setHelpOpened(false);
}
