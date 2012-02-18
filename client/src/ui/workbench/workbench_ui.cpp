#include "workbench_ui.h"
#include <cassert>
#include <cmath> /* For std::floor. */

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsProxyWidget>
#include <QGraphicsLinearLayout>
#include <QStyle>
#include <QApplication>
#include <QSettings>
#include <QMenu>
#include <QLabel>

#include <utils/common/event_signalizer.h>
#include <utils/common/scoped_value_rollback.h>

#include <core/dataprovider/abstract_streamdataprovider.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/layout_resource.h>

#include <camera/resource_display.h>

#include <ui/animation/viewport_animator.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/widget_opacity_animator.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/ui_elements_instrument.h>
#include <ui/graphics/instruments/animation_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/bounding_instrument.h>
#include <ui/graphics/instruments/activity_listener_instrument.h>
#include <ui/graphics/instruments/fps_counting_instrument.h>
#include <ui/graphics/items/image_button_widget.h>
#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/items/masked_proxy_widget.h>
#include <ui/graphics/items/clickable_widget.h>
#include <ui/graphics/items/standard/graphicslabel.h>
#include <ui/graphics/items/controls/navigationitem.h>

#include <ui/processors/hover_processor.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action.h>
#include <ui/actions/action_meta_types.h>
#include <ui/widgets/resource_tree_widget.h>
#include <ui/widgets/layout_tab_bar.h>
#include <ui/style/skin.h>
#include <ui/mixins/render_watch_mixin.h>

#include "camera/camera.h"
#include "openal/qtvaudiodevice.h"
#include "core/resourcemanagment/resource_pool.h"
#include "ui/ui_common.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"

#include "workbench.h"
#include "workbench_display.h"
#include "workbench_layout.h"


Q_DECLARE_METATYPE(VariantAnimator *)

namespace {

    QnImageButtonWidget *newActionButton(QAction *action, QGraphicsItem *parent = NULL) {
        int baseSize = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, NULL, NULL);

        qreal scaleFactor = 0.85;
        qreal height = baseSize / scaleFactor;
        qreal width = height * SceneUtility::aspectRatio(action->icon().actualSize(QSize(1024, 1024)));

        QnZoomingImageButtonWidget *button = new QnZoomingImageButtonWidget(parent);
        button->setScaleFactor(scaleFactor);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::ToolButton);
        button->setMaximumSize(width, height);
        button->setMinimumSize(width, height);
        button->setDefaultAction(action);
        button->setAnimationSpeed(4.0);
        button->setCached(true);

        return button;
    }

    QnImageButtonWidget *newShowHideButton(QGraphicsItem *parent = NULL) {
        QnImageButtonWidget *button = new QnImageButtonWidget(parent);
        button->resize(15, 45);
        button->setPixmap(QnImageButtonWidget::DEFAULT, Skin::pixmap("slide_right.png"));
        button->setPixmap(QnImageButtonWidget::HOVERED, Skin::pixmap("slide_right_hovered.png"));
        button->setPixmap(QnImageButtonWidget::CHECKED, Skin::pixmap("slide_left.png"));
        button->setPixmap(QnImageButtonWidget::CHECKED | QnImageButtonWidget::HOVERED, Skin::pixmap("slide_left_hovered.png"));
        button->setCheckable(true);
        button->setAnimationSpeed(4.0);
        return button;
    }

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

} // anonymous namespace


QnWorkbenchUi::QnWorkbenchUi(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent),
    m_display(display),
    m_manager(display->instrumentManager()),
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
    m_ignoreClickEvent(false)
{
    memset(m_widgetByRole, 0, sizeof(m_widgetByRole));

    /* Install and configure instruments. */
    m_fpsCountingInstrument = new FpsCountingInstrument(333, this);
    m_uiElementsInstrument = new UiElementsInstrument(this);
    m_controlsActivityInstrument = new ActivityListenerInstrument(hideConstrolsTimeoutMSec, this);

    m_manager->installInstrument(m_uiElementsInstrument, InstallationMode::INSTALL_BEFORE, m_display->paintForwardingInstrument());
    m_manager->installInstrument(m_fpsCountingInstrument, InstallationMode::INSTALL_BEFORE, m_display->paintForwardingInstrument());
    m_manager->installInstrument(m_controlsActivityInstrument);

    connect(m_controlsActivityInstrument, SIGNAL(activityStopped()),                                                                this,                           SLOT(at_activityStopped()));
    connect(m_controlsActivityInstrument, SIGNAL(activityResumed()),                                                                this,                           SLOT(at_activityStarted()));
    connect(m_fpsCountingInstrument,    SIGNAL(fpsChanged(qreal)),                                                                  this,                           SLOT(at_fpsChanged(qreal)));

    /* Create controls. */
    m_controlsWidget = m_uiElementsInstrument->widget(); /* Setting an ItemIsPanel flag on this item prevents focusing on graphics widgets. Don't set it. */
    m_display->setLayer(m_controlsWidget, QnWorkbenchDisplay::UI_ELEMENTS_LAYER);

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

    m_display->view()->addAction(qnAction(Qn::ShowFpsAction));
    connect(qnAction(Qn::ShowFpsAction),SIGNAL(toggled(bool)),                                                                      this,                           SLOT(setFpsVisible(bool)));
    connect(m_fpsItem,                  SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_fpsItem_geometryChanged()));
    setFpsVisible(false);


    /* Tree widget. */
    m_treeWidget = new QnResourceTreeWidget();
    m_treeWidget->setAttribute(Qt::WA_TranslucentBackground);
    {
        QPalette palette = m_treeWidget->palette();
        palette.setColor(QPalette::Window, Qt::transparent);
        palette.setColor(QPalette::Base, Qt::transparent);
        m_treeWidget->setPalette(palette);
    }
    m_treeWidget->setWorkbench(display->workbench());

    m_treeBackgroundItem = new QGraphicsWidget(m_controlsWidget);
    m_treeBackgroundItem->setAutoFillBackground(true);
    {
        QPalette palette = m_treeBackgroundItem->palette();

        QLinearGradient gradient(0, 0, 1, 0);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(0.0,  QColor(0, 0, 0, 255));
        gradient.setColorAt(0.995, QColor(0, 0, 0, 64));
        gradient.setColorAt(1.0,  QColor(0, 0, 0, 255));
        gradient.setSpread(QGradient::RepeatSpread);

        palette.setBrush(QPalette::Window, QBrush(gradient));
        m_treeBackgroundItem->setPalette(palette);
    }

    m_treeItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_treeItem->setWidget(m_treeWidget);
    m_treeItem->setFocusPolicy(Qt::StrongFocus);

    m_treePinButton = new QnImageButtonWidget(m_controlsWidget);
    m_treePinButton->resize(24, 24);
    m_treePinButton->setPixmap(QnImageButtonWidget::DEFAULT, Skin::pixmap("pin.png"));
    m_treePinButton->setPixmap(QnImageButtonWidget::CHECKED, Skin::pixmap("unpin.png"));
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
    m_treeHidingProcessor->setHoverLeaveDelay(1000);
    m_treeHidingProcessor->setFocusLeaveDelay(1000);

    m_treeShowingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_treeShowingProcessor->addTargetItem(m_treeShowButton);
    m_treeShowingProcessor->setHoverEnterDelay(250);

    m_treeXAnimator = new VariantAnimator(this);
    m_treeXAnimator->setTimer(display->animationInstrument()->animationTimer());
    m_treeXAnimator->setTargetObject(m_treeItem);
    m_treeXAnimator->setAccessor(new PropertyAccessor("x"));
    m_treeXAnimator->setSpeed(m_treeItem->size().width() * 2.0);
    m_treeXAnimator->setTimeLimit(500);

    m_treeOpacityAnimatorGroup = new AnimatorGroup(this);
    m_treeOpacityAnimatorGroup->setTimer(display->animationInstrument()->animationTimer());
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treeItem));
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treeBackgroundItem)); /* Speed of 1.0 is OK here. */
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treeShowButton));
    m_treeOpacityAnimatorGroup->addAnimator(opacityAnimator(m_treePinButton));

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


    /* Navigation slider. */
    m_sliderShowButton = newShowHideButton(m_controlsWidget);
    {
        QTransform transform;
        transform.rotate(-90);
        m_sliderShowButton->setTransform(transform);
    }

    m_sliderItem = new NavigationItem(m_controlsWidget);
    m_sliderShowButton->setFocusProxy(m_sliderItem);

    m_sliderOpacityProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_sliderOpacityProcessor->addTargetItem(m_sliderItem);
    m_sliderOpacityProcessor->addTargetItem(m_sliderShowButton);

    m_sliderYAnimator = new VariantAnimator(this);
    m_sliderYAnimator->setTimer(display->animationInstrument()->animationTimer());
    m_sliderYAnimator->setTargetObject(m_sliderItem);
    m_sliderYAnimator->setAccessor(new PropertyAccessor("y"));
    m_sliderYAnimator->setSpeed(m_sliderItem->size().height() * 2.0);
    m_sliderYAnimator->setTimeLimit(500);

    m_sliderOpacityAnimatorGroup = new AnimatorGroup(this);
    m_sliderOpacityAnimatorGroup->setTimer(display->animationInstrument()->animationTimer());
    m_sliderOpacityAnimatorGroup->addAnimator(opacityAnimator(m_sliderItem));
    m_sliderOpacityAnimatorGroup->addAnimator(opacityAnimator(m_sliderShowButton)); /* Speed of 1.0 is OK here. */

    connect(m_sliderShowButton,         SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_sliderShowButton_toggled(bool)));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateSliderOpacity()));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateSliderOpacity()));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateControlsVisibility()));
    connect(m_sliderOpacityProcessor,   SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateControlsVisibility()));
    connect(m_sliderItem,               SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_sliderItem_geometryChanged()));
    connect(m_sliderItem,               SIGNAL(actualCameraChanged(CLVideoCamera *)),                                               this,                           SLOT(updateControlsVisibility()));
    //connect(m_sliderItem,           SIGNAL(playbackMaskChanged(const QnTimePeriodList &)),                                      m_display,                      SIGNAL(playbackMaskChanged(const QnTimePeriodList &)));
    connect(m_sliderItem,               SIGNAL(enableItemSync(bool)),                                                               m_display,                      SIGNAL(enableItemSync(bool)));
    connect(m_sliderItem,               SIGNAL(exportRange(CLVideoCamera*, qint64, qint64)),                                        this,                           SLOT(at_exportMediaRange(CLVideoCamera*, qint64, qint64)));


    /* Title bar. */
    m_titleBackgroundItem = new QGraphicsWidget(m_controlsWidget);
    m_titleBackgroundItem->setAutoFillBackground(true);
    {
        QPalette palette = m_titleBackgroundItem->palette();

        QLinearGradient gradient(0, 0, 0, 1);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(0.0,  QColor(0, 0, 0, 255));
        gradient.setColorAt(0.95, QColor(0, 0, 0, 64));
        gradient.setColorAt(1.0,  QColor(0, 0, 0, 255));
        gradient.setSpread(QGradient::RepeatSpread);

        palette.setBrush(QPalette::Window, QBrush(gradient));
        m_titleBackgroundItem->setPalette(palette);
    }

    m_titleItem = new QnClickableWidget(m_controlsWidget);
    m_titleItem->setPos(0.0, 0.0);
    m_titleItem->setClickableButtons(Qt::LeftButton);

    m_tabBarItem = new QGraphicsProxyWidget(m_controlsWidget);
    m_tabBarItem->setCacheMode(QGraphicsItem::ItemCoordinateCache);
    
    QnLayoutTabBar *tabBarWidget = new QnLayoutTabBar();
    tabBarWidget->setAttribute(Qt::WA_TranslucentBackground);
    tabBarWidget->setWorkbench(display->workbench());
    m_tabBarItem->setWidget(tabBarWidget);

    m_mainMenuButton = newActionButton(qnAction(Qn::MainMenuAction));

    QGraphicsLinearLayout *titleLayout = new QGraphicsLinearLayout();
    titleLayout->setSpacing(2);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->addItem(m_mainMenuButton);
    titleLayout->addItem(m_tabBarItem);
    titleLayout->addItem(newActionButton(qnAction(Qn::NewTabAction)));
    titleLayout->addStretch(0x1000);
    titleLayout->addItem(newActionButton(qnAction(Qn::ConnectionSettingsAction)));
    titleLayout->addItem(newActionButton(qnAction(Qn::PreferencesAction)));
    titleLayout->addItem(newActionButton(qnAction(Qn::FullscreenAction)));
    titleLayout->addItem(newActionButton(qnAction(Qn::ExitAction)));
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
    m_titleYAnimator->setTimer(display->animationInstrument()->animationTimer());
    m_titleYAnimator->setTargetObject(m_titleItem);
    m_titleYAnimator->setAccessor(new PropertyAccessor("y"));
    m_titleYAnimator->setSpeed(m_titleItem->size().height() * 2.0);
    m_titleYAnimator->setTimeLimit(500);

    m_titleOpacityAnimatorGroup = new AnimatorGroup(this);
    m_titleOpacityAnimatorGroup->setTimer(display->animationInstrument()->animationTimer());
    m_titleOpacityAnimatorGroup->addAnimator(opacityAnimator(m_titleItem));
    m_titleOpacityAnimatorGroup->addAnimator(opacityAnimator(m_titleBackgroundItem)); /* Speed of 1.0 is OK here. */
    m_titleOpacityAnimatorGroup->addAnimator(opacityAnimator(m_titleShowButton));

    connect(tabBarWidget,               SIGNAL(closeRequested(QnWorkbenchLayout *)),                                                this,                           SIGNAL(closeRequested(QnWorkbenchLayout *)));
    connect(m_titleShowButton,          SIGNAL(toggled(bool)),                                                                      this,                           SLOT(at_titleShowButton_toggled(bool)));
    connect(m_titleOpacityProcessor,    SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateTitleOpacity()));
    connect(m_titleOpacityProcessor,    SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateTitleOpacity()));
    connect(m_titleOpacityProcessor,    SIGNAL(hoverEntered()),                                                                     this,                           SLOT(updateControlsVisibility()));
    connect(m_titleOpacityProcessor,    SIGNAL(hoverLeft()),                                                                        this,                           SLOT(updateControlsVisibility()));
    connect(m_titleItem,                SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_titleItem_geometryChanged()));
    connect(m_titleItem,                SIGNAL(doubleClicked()),                                                                    qnAction(Qn::FullscreenAction), SLOT(toggle()));
    connect(qnAction(Qn::MainMenuAction),SIGNAL(triggered()),                                                                       this,                           SLOT(at_mainMenuAction_triggered()));


    /* Help window. */
    m_helpBackgroundItem = new QGraphicsWidget(m_controlsWidget);
    m_helpBackgroundItem->setAutoFillBackground(true);
    {
        QPalette palette = m_helpBackgroundItem->palette();

        QLinearGradient gradient(0, 0, 1, 0);
        gradient.setCoordinateMode(QGradient::ObjectBoundingMode);
        gradient.setColorAt(1.0,  QColor(0, 0, 0, 255));
        gradient.setColorAt(0.005, QColor(0, 0, 0, 64));
        gradient.setColorAt(0.0,  QColor(0, 0, 0, 255));
        gradient.setSpread(QGradient::RepeatSpread);

        palette.setBrush(QPalette::Window, QBrush(gradient));
        m_helpBackgroundItem->setPalette(palette);
    }

    QLabel *m_helpWidget = new QLabel();
    m_helpWidget->setAttribute(Qt::WA_TranslucentBackground);
    {
        QPalette palette = m_helpWidget->palette();
        palette.setColor(QPalette::Window, Qt::transparent);
        palette.setColor(QPalette::Base, Qt::transparent);
        m_helpWidget->setPalette(palette);
    }
    m_helpWidget->setTextFormat(Qt::RichText);
    m_helpWidget->setText(trUtf8("<div style=\"color:red;font-size:100px\" align=\"center\">:)</div>"));
    m_helpWidget->setMinimumWidth(200);
    m_helpWidget->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_helpItem = new QnMaskedProxyWidget(m_controlsWidget);
    m_helpItem->setWidget(m_helpWidget);

    m_helpPinButton = new QnImageButtonWidget(m_controlsWidget);
    m_helpPinButton->resize(24, 24);
    m_helpPinButton->setPixmap(QnImageButtonWidget::DEFAULT, Skin::pixmap("pin.png"));
    m_helpPinButton->setPixmap(QnImageButtonWidget::CHECKED, Skin::pixmap("unpin.png"));
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
    m_helpHidingProcessor->setHoverLeaveDelay(1000);
    m_helpHidingProcessor->setFocusLeaveDelay(1000);

    m_helpShowingProcessor = new HoverFocusProcessor(m_controlsWidget);
    m_helpShowingProcessor->addTargetItem(m_helpShowButton);
    m_helpShowingProcessor->setHoverEnterDelay(250);

    m_helpXAnimator = new VariantAnimator(this);
    m_helpXAnimator->setTimer(display->animationInstrument()->animationTimer());
    m_helpXAnimator->setTargetObject(m_helpItem);
    m_helpXAnimator->setAccessor(new PropertyAccessor("x"));
    m_helpXAnimator->setSpeed(m_helpItem->size().width() * 2.0);
    m_helpXAnimator->setTimeLimit(500);

    m_helpOpacityAnimatorGroup = new AnimatorGroup(this);
    m_helpOpacityAnimatorGroup->setTimer(display->animationInstrument()->animationTimer());
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



    /* Connect to display. */
    connect(m_display,                  SIGNAL(widgetChanged(QnWorkbench::ItemRole)),                                               this,                           SLOT(at_display_widgetChanged(QnWorkbench::ItemRole)));
    connect(m_display,                  SIGNAL(widgetAdded(QnResourceWidget *)),                                                    this,                           SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(m_display,                  SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)),                                         this,                           SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(m_display->renderWatcher(), SIGNAL(displayingStateChanged(QnAbstractRenderer *, bool)),                                 this,                           SLOT(at_renderWatcher_displayingStateChanged(QnAbstractRenderer *, bool)));


    /* Init fields. */
    setFlags(HIDE_WHEN_NORMAL | HIDE_WHEN_ZOOMED | AFFECT_MARGINS_WHEN_ZOOMED | AFFECT_MARGINS_WHEN_NORMAL);

    setSliderOpened(true, false);
    setSliderVisible(false, false);
    setTreeOpened(false, false);
    setTreeVisible(true, false);
    setTitleOpened(true, false);
    setTitleVisible(true, false);
    setTitleUsed(false);
    setHelpOpened(false, false);
    setHelpVisible(true, false);
}

QnWorkbenchUi::~QnWorkbenchUi() {
    return;
}

QVariant QnWorkbenchUi::target(QnAction *action) {
    /* Determine current scope. */
    Qn::ActionScope scope = Qn::MainScope;

    QGraphicsItem *focusItem = display()->scene()->focusItem();
    if(focusItem == m_treeItem) {
        scope = Qn::TreeScope;
    } else if(focusItem == m_sliderItem) {
        scope = Qn::SliderScope;
    } else if(!focusItem || dynamic_cast<QnResourceWidget *>(focusItem)) {
        scope = Qn::SceneScope;
    }

    /* Compare to action's scope. */
    if(!(action->scope() & scope))
        return QVariant();
    
    /* Get items. */
    switch(scope) {
    case Qn::TreeScope:
        return QVariant::fromValue(m_treeWidget->selectedResources());
    case Qn::SliderScope: {
        QnResourceList result;
        CLVideoCamera *camera = m_sliderItem->videoCamera();
        if(camera != NULL)
            result.push_back(camera->resource());
        
        return QVariant::fromValue(result);
    }
    case Qn::SceneScope:
        return QVariant::fromValue(QnActionMetaTypes::widgets(display()->scene()->selectedItems()));
    default:
        return QVariant();
    }
}

QnWorkbenchDisplay *QnWorkbenchUi::display() const {
    return m_display;
}

QnWorkbench *QnWorkbenchUi::workbench() const {
    return m_display->workbench();
}

void QnWorkbenchUi::setTreeOpened(bool opened, bool animate)
{
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

void QnWorkbenchUi::setHelpOpened(bool opened, bool animate)
{
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
        QWidget *widget = m_tabBarItem->widget();
        m_tabBarItem->setWidget(NULL);
        m_tabBarItem->setWidget(widget);
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
    if(m_inactive) {
        bool hovered = m_sliderOpacityProcessor->isHovered() || m_treeOpacityProcessor->isHovered() || m_titleOpacityProcessor->isHovered() || m_helpOpacityProcessor->isHovered();
        setSliderVisible(hovered, animate);
        setTreeVisible(hovered, animate);
        setTitleVisible(hovered, animate);
        setHelpVisible(hovered, animate);
    } else {
        setSliderVisible(m_sliderItem->videoCamera() != NULL, animate);
        setTreeVisible(true, animate);
        setTitleVisible(true, animate);
        setHelpVisible(true, animate);
    }
}

QRectF QnWorkbenchUi::updatedTreeGeometry(const QRectF &treeGeometry, const QRectF &titleGeometry, const QRectF &sliderGeometry) {
    QPointF pos(
        treeGeometry.x(),
        ((!m_titleVisible || !m_titleUsed) && m_treeVisible) ? 0.0 : qMax(titleGeometry.bottom(), 0.0)
    );
    QSizeF size(
        treeGeometry.width(),
        ((!m_sliderVisible && m_treeVisible) ? m_controlsWidgetRect.bottom() : qMin(sliderGeometry.y(), m_controlsWidgetRect.bottom())) - pos.y()
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
        ((!m_titleVisible || !m_titleUsed) && m_helpVisible) ? 0.0 : qMax(titleGeometry.bottom(), 0.0)
    );
    QSizeF size(
        helpGeometry.width(),
        ((!m_sliderVisible && m_helpVisible) ? m_controlsWidgetRect.bottom() : qMin(sliderGeometry.y(), m_controlsWidgetRect.bottom())) - pos.y()
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

QMargins QnWorkbenchUi::calculateViewportMargins(qreal treeX, qreal treeW, qreal titleY, qreal titleH, qreal sliderY, qreal helpY) {
    return QMargins(
        m_treePinned ? std::floor(qMax(0.0, treeX + treeW)) : 0.0,
        std::floor(qMax(0.0, titleY + titleH)),
        std::floor(qMax(0.0, m_helpPinned ? m_controlsWidgetRect.right() - helpY : 0.0)),
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

    qnAction(Qn::ShowFpsAction)->setChecked(fpsVisible);
}

void QnWorkbenchUi::setTreeShowButtonUsed(bool used) {
    if(used) {
        m_treeShowButton->setAcceptedMouseButtons(Qt::LeftButton);
    } else {
        m_treeShowButton->setAcceptedMouseButtons(0);
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
    bool affectMargins;
    if(m_widgetByRole[QnWorkbench::ZOOMED] != NULL) {
        affectMargins = m_flags & AFFECT_MARGINS_WHEN_ZOOMED;
    } else {
        affectMargins = m_flags & AFFECT_MARGINS_WHEN_NORMAL;
    }

    if(!affectMargins) {
        m_display->setViewportMargins(QMargins(0, 0, 0, 0));
    } else {
        m_display->setViewportMargins(calculateViewportMargins(
            m_treeXAnimator->isRunning() ? m_treeXAnimator->targetValue().toReal() : m_treeItem->pos().x(),
            m_treeItem->size().width(),
            m_titleYAnimator->isRunning() ? m_titleYAnimator->targetValue().toReal() : m_titleItem->pos().y(),
            m_titleItem->size().height(),
            m_sliderYAnimator->isRunning() ? m_sliderYAnimator->targetValue().toReal() : m_sliderItem->pos().y(),
            m_helpXAnimator->isRunning() ? m_helpXAnimator->targetValue().toReal() : m_helpItem->pos().y()
        ));
    }
}

void QnWorkbenchUi::updateActivityInstrumentState() {
    bool zoomed = m_widgetByRole[QnWorkbench::ZOOMED] != NULL;

    if(zoomed) {
        m_controlsActivityInstrument->setEnabled(m_flags & HIDE_WHEN_ZOOMED);
    } else {
        m_controlsActivityInstrument->setEnabled(m_flags & HIDE_WHEN_NORMAL);
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchUi::at_fpsChanged(qreal fps) {
    m_fpsItem->setText(QString::number(fps, 'g', 4));
    m_fpsItem->resize(m_fpsItem->effectiveSizeHint(Qt::PreferredSize));
}

void QnWorkbenchUi::at_activityStopped() {
    m_inactive = true;

    updateControlsVisibility(true);
}

void QnWorkbenchUi::at_activityStarted() {
    m_inactive = false;

    updateControlsVisibility(true);
}

void QnWorkbenchUi::at_mainMenuAction_triggered() {
    if(!m_mainMenuButton->isVisible())
        return;

    /* On multi-monitor setups where monitor sizes differ,
     * these coordinates can be negative, so we shouldn't adjust them. */
    QPoint pos = m_display->view()->mapToGlobal(m_display->view()->mapFromScene(m_mainMenuButton->mapToScene(m_mainMenuButton->rect().bottomLeft())));

    QScopedPointer<QMenu> menu(qnMenu->newMenu(Qn::MainScope));
    menu->move(pos);
    menu->exec();
}

void QnWorkbenchUi::at_renderWatcher_displayingStateChanged(QnAbstractRenderer *renderer, bool displaying) {
    QnResourceWidget *widget = m_display->widget(renderer);
    if(widget != NULL)
        m_sliderItem->onDisplayingStateChanged(widget->display()->dataProvider()->getResource(), displaying);
}

void QnWorkbenchUi::at_display_widgetChanged(QnWorkbench::ItemRole role) {
    QnResourceWidget *oldWidget = m_widgetByRole[role];
    QnResourceWidget *newWidget = m_display->widget(role);
    m_widgetByRole[role] = newWidget;

    /* Tune activity listener instrument. */
    if(role == QnWorkbench::ZOOMED) {
        updateActivityInstrumentState();
        updateViewportMargins();
    }

    /* Update navigation item's target. */
    QnResourceWidget *targetWidget = m_widgetByRole[QnWorkbench::ZOOMED];
    if(targetWidget == NULL)
        targetWidget = m_widgetByRole[QnWorkbench::RAISED];
    m_sliderItem->setVideoCamera(targetWidget == NULL ? NULL : targetWidget->display()->camera());
}

void QnWorkbenchUi::at_display_widgetAdded(QnResourceWidget *widget) {
    if(widget->display() == NULL || widget->display()->camera() == NULL)
        return;

    QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>();
#ifndef DEBUG_MOTION
    if(cameraResource != NULL)
#endif
    {
        connect(widget, SIGNAL(motionRegionSelected(QnResourcePtr, QnAbstractArchiveReader*, QRegion)), m_sliderItem, SLOT(loadMotionPeriods(QnResourcePtr, QnAbstractArchiveReader*, QRegion)));
        connect(m_sliderItem, SIGNAL(clearMotionSelection()), widget, SLOT(clearMotionSelection()));
        m_sliderItem->addReserveCamera(widget->display()->camera());
    }
}

void QnWorkbenchUi::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(widget->display() == NULL || widget->display()->camera() == NULL)
        return;

    QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>();
    if(cameraResource != NULL)
        m_sliderItem->removeReserveCamera(widget->display()->camera());
}

void QnWorkbenchUi::at_exportMediaRange(CLVideoCamera* camera, qint64 startTimeMs, qint64 endTimeMs)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("export"));
    QString previousDir = settings.value(QLatin1String("previousDir")).toString();
    QString dateFormat = startTimeMs < 1000ll * 100000 ? QLatin1String("hh-mm-ss") : QLatin1String("dd-mmm-yyyy hh-mm-ss");
    QString suggetion = camera->getDevice()->getUniqueId();

    QString fileName;
    while (1)
    {
        fileName = QFileDialog::getSaveFileName(m_display->view(), tr("Export Video As..."),
            previousDir + QLatin1Char('/') + suggetion,
            tr("Matroska(*.mkv)"),
            0,
            QFileDialog::DontUseNativeDialog);
        if (fileName.isEmpty())
            return;
        QString fullName = fileName;
        if (!fullName.toLower().endsWith(QLatin1String(".mkv")))
            fullName += QLatin1String(".mkv");
        if (QFile::exists(fullName))
        {
            QString shortName = QFileInfo(fullName).baseName();
            QMessageBox msgBox(QMessageBox::Information, tr("Confirm Save As"), QString("File '%1' already exists. Overwrite?").arg(shortName),
                               QMessageBox::Yes | QMessageBox::No, m_display->view());
            if (msgBox.exec() == QMessageBox::Yes)
            {
                if (!QFile::remove(fullName))
                {
                    UIOKMessage(m_display->view(), "Can't overwrite file", QString("File '%1' is used by another process. Try another name.").arg(shortName));
                    continue;
                }
                break;
            }
        }
        else 
            break;
    }

    settings.setValue(QLatin1String("previousDir"), QFileInfo(fileName).absolutePath());

    QProgressDialog *exportProgressDialog = new QProgressDialog(m_display->view());
    exportProgressDialog->setAttribute(Qt::WA_DeleteOnClose);
    exportProgressDialog->setAttribute(Qt::WA_QuitOnClose);
    exportProgressDialog->setWindowFlags(Qt::WindowStaysOnTopHint);
    exportProgressDialog->setWindowTitle(tr("Exporting Video"));
    exportProgressDialog->setLabelText(tr("Exporting to \"%1\"...").arg(fileName));
    exportProgressDialog->setRange(0, 100);
    exportProgressDialog->setMinimumDuration(1000);
    connect(exportProgressDialog, SIGNAL(canceled()), camera, SLOT(stopExport()));
    connect(exportProgressDialog, SIGNAL(canceled()), exportProgressDialog, SLOT(reject()));
    connect(exportProgressDialog, SIGNAL(finished(int)), exportProgressDialog, SLOT(deleteLater()));

    connect(camera, SIGNAL(exportProgress(int)), exportProgressDialog, SLOT(setValue(int)));
    connect(camera, SIGNAL(exportFailed(QString)), exportProgressDialog, SLOT(reject()));
    connect(camera, SIGNAL(exportFinished(QString)), exportProgressDialog, SLOT(accept()));

    camera->disconnect(this);
    connect(camera, SIGNAL(exportFailed(QString)), this, SLOT(at_exportFailed(QString)));
    connect(camera, SIGNAL(exportFinished(QString)), this, SLOT(at_exportFinished(QString)));

    camera->exportMediaPeriodToFile(startTimeMs*1000ll, endTimeMs*1000ll, fileName);
}

void QnWorkbenchUi::at_exportFinished(QString fileName)
{
    QnAviResourcePtr file(new QnAviResource(fileName));
    qnResPool->addResource(file);

    QMessageBox::information(0, tr("Export finished"), tr("Export successfully finished"));

}

void QnWorkbenchUi::at_exportFailed(QString errMessage)
{
    CLVideoCamera *cam = dynamic_cast<CLVideoCamera *>(sender()); // ### qobject_cast ?
    if (cam)
        cam->stopExport();
    QMessageBox::warning(0, tr("Can not export video"), errMessage, QMessageBox::Ok, QMessageBox::NoButton);
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
    updateTreeGeometry();
    updateHelpGeometry();

    updateViewportMargins();

    QRectF geometry = m_sliderItem->geometry();

    m_sliderShowButton->setPos(QPointF(
        (geometry.left() + geometry.right() - m_titleShowButton->size().height()) / 2,
        qMin(m_controlsWidgetRect.bottom(), geometry.top())
    ));
}

void QnWorkbenchUi::at_treeWidget_activated(const QnResourcePtr &resource) {
    if(resource.isNull())
        return;

    if(resource->checkFlags(QnResource::layout)) {
        QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();
        if(layoutResource) {
            QnWorkbenchLayout *layout = QnWorkbenchLayout::layout(layoutResource);
            if(layout == NULL) {
                layout = new QnWorkbenchLayout(layoutResource, workbench());
                workbench()->addLayout(layout);
            }

            workbench()->setCurrentLayout(layout);
        }
    } else if(resource->checkFlags(QnResource::media)) {
        QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId(), QUuid::createUuid());
        item->setFlag(QnWorkbenchItem::Pinned, false);
        display()->workbench()->currentLayout()->addItem(item);
        item->adjustGeometry();
    }
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
        QTimer::singleShot(200, this, SLOT(setTreeShowButtonUsed()));
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
        //setTreeShowButtonUsed(false);
        //QTimer::singleShot(200, this, SLOT(setTreeShowButtonUsed()));
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
