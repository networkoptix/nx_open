#include "workbench_controller.h"
#include <cassert>
#include <cmath> /* For std::floor. */
#include <limits>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGLWidget>
#include <QGraphicsLinearLayout>
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QLabel>
#include <QPropertyAnimation>
#include <QFileInfo>
#include <QSettings>
#include <QFileDialog>

#include <core/resourcemanagment/security_cam_resource.h>
#include <core/resource/directory_browser.h>
#include <core/resourcemanagment/resource_pool.h>

#include <camera/resource_display.h>
#include <camera/camdisplay.h>

#include <ui/screen_recording/screen_recorder.h>
#include <ui/preferences/preferences_wnd.h>

#include <ui/animation/viewport_animator.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/wheel_zoom_instrument.h>
#include <ui/graphics/instruments/rubber_band_instrument.h>
#include <ui/graphics/instruments/drag_instrument.h>
#include <ui/graphics/instruments/rotation_instrument.h>
#include <ui/graphics/instruments/click_instrument.h>
#include <ui/graphics/instruments/bounding_instrument.h>
#include <ui/graphics/instruments/stop_instrument.h>
#include <ui/graphics/instruments/stop_accepted_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/instruments/selection_fixup_instrument.h>
#include <ui/graphics/instruments/drop_instrument.h>
#include <ui/graphics/instruments/resizing_instrument.h>
#include <ui/graphics/instruments/ui_elements_instrument.h>
#include <ui/graphics/instruments/resize_hover_instrument.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/instruments/animation_instrument.h>
#include <ui/graphics/instruments/selection_overlay_hack_instrument.h>

#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/items/grid_item.h>

#include <ui/videoitem/navigationitem.h>

#include <ui/context_menu/menu_wrapper.h>

#include <file_processor.h>

#include "grid_walker.h"
#include "workbench_layout.h"
#include "workbench_item.h"
#include "workbench_grid_mapper.h"
#include "workbench.h"
#include "workbench_display.h"
#include "ui/device_settings/camera_schedule_widget.h"
#include "ui/device_settings/camera_motionmask_widget.h"

Q_DECLARE_METATYPE(VariantAnimator *);

namespace {
    QAction *newAction(const QString &text, const QString &shortcut, QObject *parent = NULL) {
        QAction *result = new QAction(text, parent);
        result->setShortcut(shortcut);
        return result;
    }

    QRegion createRoundRegion(int rSmall, int rLarge, const QRect &rect) {
        QRegion region;

        int circleX = rLarge;

        int circleY = rSmall-1;
        for (int y = 0; y < qMin(rect.height(), rSmall); ++y)
        {
            // calculate circle Point
            int x = circleX - std::sqrt((double) rLarge*rLarge - (circleY-y)*(circleY-y)) + 0.5;
            region += QRect(x,y, rect.width()-x*2,1);
        }
        for (int y = qMin(rect.height(), rSmall); y < rect.height() - rSmall; ++y)
            region += QRect(0,y, rect.width(),1);

        circleY = rect.height() - rSmall;
        for (int y = rect.height() - rSmall; y < rect.height(); ++y)
        {
            // calculate circle Point
            int x = circleX - std::sqrt((double) rLarge*rLarge - (circleY-y)*(circleY-y)) + 0.5;
            region += QRect(x,y, rect.width()-x*2,1);
        }
        return region;
    }

    QSize bestSingleBoundedSize(QnWorkbenchGridMapper *mapper, int bound, Qt::Orientation boundOrientation, qreal aspectRatio) {
        QSizeF sceneSize = mapper->mapFromGrid(boundOrientation == Qt::Horizontal ? QSize(bound, 0) : QSize(0, bound));
        if(boundOrientation == Qt::Horizontal) {
            sceneSize.setHeight(sceneSize.width() / aspectRatio);
        } else {
            sceneSize.setWidth(sceneSize.height() * aspectRatio);
        }

        QSize gridSize0 = mapper->mapToGrid(sceneSize);
        QSize gridSize1 = gridSize0;
        if(boundOrientation == Qt::Horizontal) {
            gridSize1.setHeight(qMax(gridSize1.height() - 1, 1));
        } else {
            gridSize1.setWidth(qMax(gridSize1.width() - 1, 1));
        }

        qreal distance0 = std::abs(std::log(SceneUtility::aspectRatio(mapper->mapFromGrid(gridSize0)) / aspectRatio)) / 1.25; /* Prefer larger size. */
        qreal distance1 = std::abs(std::log(SceneUtility::aspectRatio(mapper->mapFromGrid(gridSize1)) / aspectRatio));
        return distance0 < distance1 ? gridSize0 : gridSize1;
    }

    QSize bestDoubleBoundedSize(QnWorkbenchGridMapper *mapper, const QSize &bound, qreal aspectRatio) {
        qreal boundAspectRatio = SceneUtility::aspectRatio(mapper->mapFromGrid(bound));

        if(aspectRatio < boundAspectRatio) {
            return bestSingleBoundedSize(mapper, bound.height(), Qt::Vertical, aspectRatio);
        } else {
            return bestSingleBoundedSize(mapper, bound.width(), Qt::Horizontal, aspectRatio);
        }
    }

    QRect bestDoubleBoundedGeometry(QnWorkbenchGridMapper *mapper, const QRect &bound, qreal aspectRatio) {
        QSize size = bestDoubleBoundedSize(mapper, bound.size(), aspectRatio);

        return QRect(
            bound.topLeft() + SceneUtility::toPoint(bound.size() - size) / 2,
            size
        );
    }

    QPoint invalidDragDelta() {
        return QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    }

    const char *opacityAnimatorPropertyName = "_qn_itemOpacityAnimator";

    const qreal widgetManipulationOpacity = 0.3;

} // anonymous namespace

QnWorkbenchController::QnWorkbenchController(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent),
    m_display(display),
    m_manager(display->instrumentManager()),
    m_resizedWidget(NULL),
    m_dragDelta(invalidDragDelta())
{
    ::memset(m_widgetByRole, 0, sizeof(m_widgetByRole));

    QEvent::Type mouseEventTypeArray[] = {
        QEvent::GraphicsSceneMousePress,
        QEvent::GraphicsSceneMouseMove,
        QEvent::GraphicsSceneMouseRelease,
        QEvent::GraphicsSceneMouseDoubleClick,
        QEvent::GraphicsSceneHoverEnter,
        QEvent::GraphicsSceneHoverMove,
        QEvent::GraphicsSceneHoverLeave
    };

    Instrument::EventTypeSet mouseEventTypes = Instrument::makeSet(mouseEventTypeArray);
    Instrument::EventTypeSet wheelEventTypes = Instrument::makeSet(QEvent::GraphicsSceneWheel);

    /* Install and configure instruments. */
    ClickInstrument *itemLeftClickInstrument = new ClickInstrument(Qt::LeftButton, 300, Instrument::ITEM, this);
    ClickInstrument *itemRightClickInstrument = new ClickInstrument(Qt::RightButton, 0, Instrument::ITEM, this);
    ClickInstrument *itemMiddleClickInstrument = new ClickInstrument(Qt::MiddleButton, 0, Instrument::ITEM, this);
    ClickInstrument *sceneClickInstrument = new ClickInstrument(Qt::LeftButton | Qt::RightButton, 0, Instrument::SCENE, this);
    m_handScrollInstrument = new HandScrollInstrument(this);
    m_wheelZoomInstrument = new WheelZoomInstrument(this);
    m_rubberBandInstrument = new RubberBandInstrument(this);
    m_rotationInstrument = new RotationInstrument(this);
    m_resizingInstrument = new ResizingInstrument(this);
    m_archiveDropInstrument = new DropInstrument(this, this);
    m_uiElementsInstrument = new UiElementsInstrument(this);
    BoundingInstrument *boundingInstrument = m_display->boundingInstrument();
    SelectionOverlayHackInstrument *selectionOverlayHackInstrument = m_display->selectionOverlayHackInstrument();
    m_dragInstrument = new DragInstrument(this);
    ForwardingInstrument *itemMouseForwardingInstrument = new ForwardingInstrument(Instrument::ITEM, mouseEventTypes, this);
    SelectionFixupInstrument *selectionFixupInstrument = new SelectionFixupInstrument(this);
    m_motionSelectionInstrument = new MotionSelectionInstrument(this);
    m_motionSelectionInstrument->recursiveDisable();

    m_rubberBandInstrument->setRubberBandZValue(m_display->layerZValue(QnWorkbenchDisplay::EFFECTS_LAYER));
    m_rotationInstrument->setRotationItemZValue(m_display->layerZValue(QnWorkbenchDisplay::EFFECTS_LAYER));
    m_resizingInstrument->setEffectiveDistance(5);

    /* Item instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::ITEM, mouseEventTypes, this));
    m_manager->installInstrument(m_resizingInstrument->resizeHoverInstrument());
    m_manager->installInstrument(selectionFixupInstrument);
    m_manager->installInstrument(itemMouseForwardingInstrument);
    m_manager->installInstrument(selectionFixupInstrument->preForwardingInstrument());
    m_manager->installInstrument(itemLeftClickInstrument);
    m_manager->installInstrument(itemRightClickInstrument);
    m_manager->installInstrument(itemMiddleClickInstrument);

    /* Scene instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::SCENE, wheelEventTypes, this));
    m_manager->installInstrument(m_wheelZoomInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::SCENE, wheelEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::SCENE, wheelEventTypes, this));

    m_manager->installInstrument(new StopInstrument(Instrument::SCENE, mouseEventTypes, this));
    m_manager->installInstrument(sceneClickInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::SCENE, mouseEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::SCENE, mouseEventTypes, this));

    /* View/viewport instruments. */
    m_manager->installInstrument(m_uiElementsInstrument, InstallationMode::INSTALL_BEFORE, m_display->paintForwardingInstrument());
    m_manager->installInstrument(m_rotationInstrument, InstallationMode::INSTALL_AFTER, m_display->transformationListenerInstrument());
    m_manager->installInstrument(m_resizingInstrument);
    m_manager->installInstrument(m_dragInstrument);
    m_manager->installInstrument(m_rubberBandInstrument);
    m_manager->installInstrument(m_handScrollInstrument);
    m_manager->installInstrument(m_archiveDropInstrument);
    m_manager->installInstrument(m_motionSelectionInstrument);

    connect(itemLeftClickInstrument,    SIGNAL(pressed(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_leftPressed(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(itemLeftClickInstrument,    SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_leftClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(itemLeftClickInstrument,    SIGNAL(doubleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                 this,                           SLOT(at_item_doubleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(itemRightClickInstrument,   SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_rightClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(itemMiddleClickInstrument,  SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_middleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(sceneClickInstrument,       SIGNAL(clicked(QGraphicsView *, const ClickInfo &)),                                        this,                           SLOT(at_scene_clicked(QGraphicsView *, const ClickInfo &)));
    connect(sceneClickInstrument,       SIGNAL(doubleClicked(QGraphicsView *, const ClickInfo &)),                                  this,                           SLOT(at_scene_doubleClicked(QGraphicsView *, const ClickInfo &)));
    connect(m_dragInstrument,           SIGNAL(dragStarted(QGraphicsView *, QList<QGraphicsItem *>)),                               this,                           SLOT(at_dragStarted(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_dragInstrument,           SIGNAL(drag(QGraphicsView *, QList<QGraphicsItem *>)),                                      this,                           SLOT(at_drag(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_dragInstrument,           SIGNAL(dragFinished(QGraphicsView *, QList<QGraphicsItem *>)),                              this,                           SLOT(at_dragFinished(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_resizingInstrument,       SIGNAL(resizingStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),          this,                           SLOT(at_resizingStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)));
    connect(m_resizingInstrument,       SIGNAL(resizing(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),                 this,                           SLOT(at_resizing(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)));
    connect(m_resizingInstrument,       SIGNAL(resizingFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),         this,                           SLOT(at_resizingFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)));
    connect(m_rotationInstrument,       SIGNAL(rotationStarted(QGraphicsView *, QnResourceWidget *)),                               this,                           SLOT(at_rotationStarted(QGraphicsView *, QnResourceWidget *)));
    connect(m_rotationInstrument,       SIGNAL(rotationFinished(QGraphicsView *, QnResourceWidget *)),                              this,                           SLOT(at_rotationFinished(QGraphicsView *, QnResourceWidget *)));

    connect(m_handScrollInstrument,     SIGNAL(scrollStarted(QGraphicsView *)),                                                     boundingInstrument,             SLOT(dontEnforcePosition(QGraphicsView *)));
    connect(m_handScrollInstrument,     SIGNAL(scrollFinished(QGraphicsView *)),                                                    boundingInstrument,             SLOT(enforcePosition(QGraphicsView *)));

    connect(m_display,                  SIGNAL(viewportGrabbed()),                                                                  m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(m_display,                  SIGNAL(viewportUngrabbed()),                                                                m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(m_display,                  SIGNAL(viewportGrabbed()),                                                                  m_wheelZoomInstrument,          SLOT(recursiveDisable()));
    connect(m_display,                  SIGNAL(viewportUngrabbed()),                                                                m_wheelZoomInstrument,          SLOT(recursiveEnable()));

    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),   itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),  itemMouseForwardingInstrument,  SLOT(recursiveEnable()));
    connect(m_dragInstrument,           SIGNAL(dragProcessStarted(QGraphicsView *)),                                                itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(m_dragInstrument,           SIGNAL(dragProcessFinished(QGraphicsView *)),                                               itemMouseForwardingInstrument,  SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QnResourceWidget *)),                        itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QnResourceWidget *)),                       itemMouseForwardingInstrument,  SLOT(recursiveEnable()));
    connect(m_handScrollInstrument,     SIGNAL(scrollProcessStarted(QGraphicsView *)),                                              itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(m_handScrollInstrument,     SIGNAL(scrollProcessFinished(QGraphicsView *)),                                             itemMouseForwardingInstrument,  SLOT(recursiveEnable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandProcessStarted(QGraphicsView *)),                                          itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandProcessFinished(QGraphicsView *)),                                         itemMouseForwardingInstrument,  SLOT(recursiveEnable()));
    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessStarted(QGraphicsView *, QnResourceWidget *)),                     itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessFinished(QGraphicsView *, QnResourceWidget *)),                    itemMouseForwardingInstrument,  SLOT(recursiveEnable()));

    connect(m_dragInstrument,           SIGNAL(dragStarted(QGraphicsView *, QList<QGraphicsItem *>)),                               selectionOverlayHackInstrument, SLOT(recursiveDisable()));
    connect(m_dragInstrument,           SIGNAL(dragFinished(QGraphicsView *, QList<QGraphicsItem *>)),                              selectionOverlayHackInstrument, SLOT(recursiveEnable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandStarted(QGraphicsView *)),                                                 selectionOverlayHackInstrument, SLOT(recursiveDisable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandFinished(QGraphicsView *)),                                                selectionOverlayHackInstrument, SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),          selectionOverlayHackInstrument, SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),         selectionOverlayHackInstrument, SLOT(recursiveEnable()));

    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),   m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),  m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),   m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),  m_rubberBandInstrument,         SLOT(recursiveEnable()));

    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QnResourceWidget *)),                        m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QnResourceWidget *)),                       m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QnResourceWidget *)),                        m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QnResourceWidget *)),                       m_rubberBandInstrument,         SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QnResourceWidget *)),                        m_resizingInstrument,           SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QnResourceWidget *)),                       m_resizingInstrument,           SLOT(recursiveEnable()));

    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessStarted(QGraphicsView *, QnResourceWidget *)),                     m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessFinished(QGraphicsView *, QnResourceWidget *)),                    m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessStarted(QGraphicsView *, QnResourceWidget *)),                     m_resizingInstrument,           SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument,  SIGNAL(selectionProcessFinished(QGraphicsView *, QnResourceWidget *)),                    m_resizingInstrument,           SLOT(recursiveEnable()));

    /* Create controls. */
    QGraphicsWidget *controlsWidget = m_uiElementsInstrument->widget();
    m_display->setLayer(controlsWidget, QnWorkbenchDisplay::UI_ELEMENTS_LAYER);

    QGraphicsLinearLayout *verticalLayout = new QGraphicsLinearLayout(Qt::Vertical);
    verticalLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    controlsWidget->setLayout(verticalLayout);
    
    m_navigationItem = new NavigationItem(controlsWidget);
   
    verticalLayout->addStretch(0x1000);
    verticalLayout->addItem(m_navigationItem);

    connect(m_navigationItem,           SIGNAL(geometryChanged()),                                                                  this,                           SLOT(at_navigationItem_geometryChanged()));

    /* Connect to display. */
    connect(m_display,                  SIGNAL(widgetChanged(QnWorkbench::ItemRole)),                                               this,                           SLOT(at_display_widgetChanged(QnWorkbench::ItemRole)));
    connect(m_display,                  SIGNAL(viewportGrabbed()),                                                                  this,                           SLOT(at_viewportGrabbed()));
    connect(m_display,                  SIGNAL(viewportUngrabbed()),                                                                this,                           SLOT(at_viewportUngrabbed()));
    connect(m_display,                  SIGNAL(widgetAdded(QnResourceWidget *)),                                                    this,                           SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(m_display,                  SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)),                                         this,                           SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));

    /* Set up context menu. */
#if 0
    QAction *addFilesAction             = newAction(tr("Add file(s)"),          tr("Ins"),          this);
    QAction *addFolderAction            = newAction(tr("Add folder"),           tr("Shift+Ins"),    this);
    QAction *addCameraAction            = newAction(tr("Add camera"),           tr("Ctrl+Ins"),     this);
    QAction *newCameraAction            = newAction(tr("New camera"),           tr("Ctrl+C"),       this);
    QAction *newLayoutAction            = newAction(tr("New layout"),           tr("Ctrl+L"),       this);
    QAction *undoAction                 = newAction(tr("Undo"),                 tr("Ctrl+Z"),       this);
    QAction *redoAction                 = newAction(tr("Redo"),                 tr("Ctrl+Shift+Z"), this);
    QAction *fitInViewAction            = newAction(tr("Fit in view"),          tr("Ctrl+V"),       this);
    QAction *fullscreenAction           = newAction(tr("Toggle fullscreen"),    tr("Ctrl+Enter"),   this);
    QAction *startStopRecordingAction   = newAction(tr("Start/stop"),           tr("Alt+R"),        this);
    QAction *recordingSettingsAction    = newAction(tr("Settings"),             QString(),          this);
    QAction *saveLayoutAction           = newAction(tr("Save layout"),          tr("Ctrl+S"),       this);
    QAction *saveLayoutAsAction         = newAction(tr("Save layout as..."),    tr("Ctrl+Shift+S"), this);
    QAction *preferencesAction          = newAction(tr("Preferences"),          tr("Ctrl+P"),       this);
    QAction *exportLayoutAction         = newAction(tr("Export layout"),        tr("Ctrl+Shift+E"), this);
#endif
    //QAction *exitAction                 = newAction(tr("Exit"),                 tr("Alt+F4"),       this);
    m_showMotionAction                  = newAction(tr("Show motion view/search grid"),          tr(""),             this);
    m_hideMotionAction                  = newAction(tr("Hide motion view/search grid"),          tr(""),             this);
    m_startRecordingAction              = newAction(tr("Start screen recording"), tr(""),           this);
    m_stopRecordingAction               = newAction(tr("Stop screen recording"), tr(""),            this);
    QAction *toggleRecordingAction      = newAction(tr("Start/stop screen recording"), tr("Alt+R"), this);
    m_recordingSettingsActions          = newAction(tr("Screen recording settings"), tr(""),        this);

    connect(m_showMotionAction,         SIGNAL(triggered(bool)),                                                    this,                           SLOT(at_showMotionAction_triggered()));
    connect(m_hideMotionAction,         SIGNAL(triggered(bool)),                                                    this,                           SLOT(at_hideMotionAction_triggered()));
    connect(m_startRecordingAction,     SIGNAL(triggered(bool)),                                                    this,                           SLOT(at_startRecordingAction_triggered()));
    connect(m_stopRecordingAction,      SIGNAL(triggered(bool)),                                                    this,                           SLOT(at_stopRecordingAction_triggered()));
    connect(toggleRecordingAction,      SIGNAL(triggered(bool)),                                                    this,                           SLOT(at_toggleRecordingAction_triggered()));
    connect(m_recordingSettingsActions, SIGNAL(triggered(bool)),                                                    this,                           SLOT(at_recordingSettingsActions_triggered()));

    m_itemContextMenu = new QMenu(display->view());

    /* Init screen recorder. */
    m_screenRecorder = new QnScreenRecorder(this);
    if(m_screenRecorder->isSupported()) {
        connect(m_screenRecorder,           SIGNAL(recordingStarted()),                                                 this,                           SLOT(at_screenRecorder_recordingStarted()));
        connect(m_screenRecorder,           SIGNAL(recordingFinished(const QString &)),                                 this,                           SLOT(at_screenRecorder_recordingFinished(const QString &)));
        connect(m_screenRecorder,           SIGNAL(error(const QString &)),                                             this,                           SLOT(at_screenRecorder_error(const QString &)));

        m_display->view()->addAction(toggleRecordingAction);
    }
    m_countdownCanceled = false;
    m_recordingLabel = 0;
    m_recordingAnimation = 0;
}

QnWorkbenchController::~QnWorkbenchController() {
    disconnect(m_screenRecorder, NULL, this, NULL);
    m_screenRecorder->stopRecording();
}

QnWorkbenchDisplay *QnWorkbenchController::display() const {
    return m_display;
}

QnWorkbench *QnWorkbenchController::workbench() const {
    return m_display->workbench();
}

QnWorkbenchLayout *QnWorkbenchController::layout() const {
    return m_display->workbench()->layout();
}

QnWorkbenchGridMapper *QnWorkbenchController::mapper() const {
    return m_display->workbench()->mapper();
}

VariantAnimator *QnWorkbenchController::opacityAnimator(QnResourceWidget *widget) {
    VariantAnimator *animator = widget->property(opacityAnimatorPropertyName).value<VariantAnimator *>();
    if(animator != NULL)
        return animator;

    animator = new VariantAnimator(widget);
    animator->setTargetObject(widget);
    animator->setTimer(display()->animationInstrument()->animationTimer());
    animator->setAccessor(new QnPropertyAccessor("opacity"));
    animator->setSpeed(1.0);
    widget->setProperty(opacityAnimatorPropertyName, QVariant::fromValue<VariantAnimator *>(animator));
    return animator;
}

void QnWorkbenchController::drop(const QUrl &url, const QPoint &gridPos, bool findAccepted) {
    drop(url.toLocalFile(), gridPos, findAccepted);
}

void QnWorkbenchController::drop(const QList<QUrl> &urls, const QPoint &gridPos, bool findAccepted) {
    QList<QString> files;
    foreach(const QUrl &url, urls)
        files.push_back(url.toLocalFile());
    drop(files, gridPos, findAccepted);
}

void QnWorkbenchController::drop(const QString &file, const QPoint &gridPos, bool findAccepted) {
    QList<QString> files;
    files.push_back(file);
    drop(files, gridPos, findAccepted);
}

void QnWorkbenchController::drop(const QList<QString> &files, const QPoint &gridPos, bool findAccepted) {
    const QList<QString> validFiles = !findAccepted ? files : QnFileProcessor::findAcceptedFiles(files);
    if(validFiles.empty())
        return;

    drop(QnFileProcessor::createResourcesForFiles(validFiles), gridPos);
}

void QnWorkbenchController::drop(const QnResourceList &resources, const QPoint &gridPos) {
    foreach(const QnResourcePtr &resource, resources)
        drop(resource, gridPos);
}

void QnWorkbenchController::drop(const QnResourcePtr &resource, const QPoint &gridPos) {
    QRect geometry(gridPos, QSize(1, 1));
    QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId());
    item->setGeometry(geometry);
    item->setFlag(QnWorkbenchItem::Pinned, false);

    layout()->addItem(item);
    QnResourceWidget *widget = display()->widget(item);

    /* Assume 4:3 AR of a single channel. In most cases, it will work fine. */
    const QnVideoResourceLayout *videoLayout = widget->display()->videoLayout();
    qreal estimatedAspectRatio = (4.0 * videoLayout->width()) / (3.0 * videoLayout->height());
    QSize gridSize;
    if(estimatedAspectRatio > 1.0) {
        gridSize = bestSingleBoundedSize(mapper(), 1, Qt::Vertical, estimatedAspectRatio);
    } else {
        gridSize = bestSingleBoundedSize(mapper(), 1, Qt::Horizontal, estimatedAspectRatio);
    }

    /* Adjust item's geometry for the new size. */
    if(gridSize != item->geometry().size()) {
        geometry = item->geometry();
        geometry.moveTopLeft(geometry.topLeft() - toPoint(gridSize - geometry.size()) / 2);
        geometry.setSize(gridSize);
        item->setGeometry(geometry);
    }

    /* Try to pin the item. */
    if(!item->setFlag(QnWorkbenchItem::Pinned, true)) {
        /* Place already taken, pick closest one. Use AR-based metric. */
        QnAspectRatioGridWalker walker(aspectRatio(display()->view()->viewport()->size()) / aspectRatio(workbench()->mapper()->step()));
        QRect newGeometry = layout()->closestFreeSlot(geometry.topLeft(), geometry.size(), &walker);
        layout()->pinItem(item, newGeometry);
    }

    display()->fitInView();
}

void QnWorkbenchController::updateGeometryDelta(QnResourceWidget *widget) {
    if(widget->item()->isPinned())
        return;

    QRectF widgetGeometry = widget->geometry();

    QRectF gridGeometry = mapper()->mapFromGrid(widget->item()->geometry());

    QSizeF step = mapper()->step();
    QRectF geometryDelta = QRectF(
        (widgetGeometry.left()   - gridGeometry.left())   / step.width(),
        (widgetGeometry.top()    - gridGeometry.top())    / step.height(),
        (widgetGeometry.width()  - gridGeometry.width())  / step.width(),
        (widgetGeometry.height() - gridGeometry.height()) / step.height()
    );

    widget->item()->setGeometryDelta(geometryDelta);
}

void QnWorkbenchController::at_resizingStarted(QGraphicsView *, QGraphicsWidget *item, const ResizingInfo &) {
    qDebug("RESIZING STARTED");

    m_resizedWidget = dynamic_cast<QnResourceWidget *>(item);
    if(m_resizedWidget == NULL)
        return;

    m_display->bringToFront(m_resizedWidget);
    m_display->gridItem()->animatedShow();
    opacityAnimator(m_resizedWidget)->animateTo(widgetManipulationOpacity);
}

void QnWorkbenchController::at_resizing(QGraphicsView *, QGraphicsWidget *item, const ResizingInfo &info) {
    if(m_resizedWidget != item)
        return;

    QnResourceWidget *widget = m_resizedWidget;

    QRectF newSceneGeometry = widget->geometry();

    /* Calculate new size in grid coordinates. */
    QSizeF gridSizeF = mapper()->mapToGridF(newSceneGeometry.size());
    QSize gridSize = QSize(
        qMax(1, qRound(gridSizeF.width())), 
        qMax(1, qRound(gridSizeF.height()))
    );

    /* Adjust for aspect ratio. */
    if(widget->hasAspectRatio()) {
        if(widget->aspectRatio() > 1.0) {
            gridSize = bestSingleBoundedSize(mapper(), gridSize.width(), Qt::Horizontal, widget->aspectRatio());
        } else {
            gridSize = bestSingleBoundedSize(mapper(), gridSize.height(), Qt::Vertical, widget->aspectRatio());
        }
    }

    /* Calculate new grid rect based on the dragged frame section. */
    QRect newResizingWidgetRect = resizeRect(widget->item()->geometry(), gridSize, info.frameSection());
    if(newResizingWidgetRect != m_resizedWidgetRect) {
        QnWorkbenchLayout::Disposition disposition;
        workbench()->layout()->canMoveItem(widget->item(), newResizingWidgetRect, &disposition);

        m_display->gridItem()->setCellState(m_resizedWidgetRect, QnGridItem::INITIAL);
        m_display->gridItem()->setCellState(disposition.free, QnGridItem::ALLOWED);
        m_display->gridItem()->setCellState(disposition.occupied, QnGridItem::DISALLOWED);

        m_resizedWidgetRect = newResizingWidgetRect;
    }
}

void QnWorkbenchController::at_resizingFinished(QGraphicsView *, QGraphicsWidget *item, const ResizingInfo &) {
    qDebug("RESIZING FINISHED");

    if(m_resizedWidget != item)
        return;

    QnResourceWidget *widget = m_resizedWidget;

    m_display->gridItem()->animatedHide();
    opacityAnimator(m_resizedWidget)->animateTo(1.0);

    /* Resize if possible. */
    QSet<QnWorkbenchItem *> entities = layout()->items(m_resizedWidgetRect);
    entities.remove(widget->item());
    if (entities.empty()) {
        widget->item()->setGeometry(m_resizedWidgetRect);
        updateGeometryDelta(widget);
    }

    m_display->synchronize(widget->item());

    /* Clean up resizing state. */
    m_display->gridItem()->setCellState(m_resizedWidgetRect, QnGridItem::INITIAL);
    m_resizedWidgetRect = QRect();
    m_resizedWidget = NULL;
}

void QnWorkbenchController::at_dragStarted(QGraphicsView *, const QList<QGraphicsItem *> &items) {
    qDebug("DRAGGING STARTED");

    /* Bring to front preserving relative order. */
    m_display->bringToFront(items);
    m_display->setLayer(items, QnWorkbenchDisplay::FRONT_LAYER);

    /* Show grid. */
    m_display->gridItem()->animatedShow();

    /* Build item lists. */
    m_draggedItems = items;
    foreach (QGraphicsItem *item, m_draggedItems) {
        QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
        if(widget == NULL)
            continue;

        m_draggedWorkbenchItems.push_back(widget->item());

        opacityAnimator(widget)->animateTo(widgetManipulationOpacity);
    }

    /* Un-raise if raised is among the dragged. */
    //QnResourceWidget *raisedWidget = display()->widget(workbench()->raisedItem());
    //if(raisedWidget != NULL && items.contains(raisedWidget))
    //    workbench()->setRaisedItem(NULL);
}

void QnWorkbenchController::at_drag(QGraphicsView *, const QList<QGraphicsItem *> &) {
    if(m_draggedWorkbenchItems.empty())
        return;

    QPoint newDragDelta = mapper()->mapToGrid(display()->widget(m_draggedWorkbenchItems[0])->geometry()).topLeft() - m_draggedWorkbenchItems[0]->geometry().topLeft();
    if(newDragDelta != m_dragDelta) {
        m_display->gridItem()->setCellState(m_dragGeometries, QnGridItem::INITIAL);

        m_dragDelta = newDragDelta;
        m_replacedWorkbenchItems.clear();
        m_dragGeometries.clear();

        /* Handle single widget case. */
        bool finished = false;
        if(m_draggedWorkbenchItems.size() == 1) {
            QnWorkbenchItem *draggedWorkbenchItem = m_draggedWorkbenchItems[0];

            /* Find item that dragged item was dropped on. */
            QPoint cursorPos = QCursor::pos();
            QSet<QnWorkbenchItem *> replacedWorkbenchItems = layout()->items(draggedWorkbenchItem->geometry().adjusted(m_dragDelta.x(), m_dragDelta.y(), m_dragDelta.x(), m_dragDelta.y()));
            if(replacedWorkbenchItems.size() == 1) {
                QnWorkbenchItem *replacedWorkbenchItem = *replacedWorkbenchItems.begin();

                /* Switch places if dropping smaller one on a bigger one. */
                if(replacedWorkbenchItem != NULL && replacedWorkbenchItem != draggedWorkbenchItem && draggedWorkbenchItem->isPinned()) {
                    QSizeF draggedSize = draggedWorkbenchItem->geometry().size();
                    QSizeF replacedSize = replacedWorkbenchItem->geometry().size();
                    if(replacedSize.width() > draggedSize.width() && replacedSize.height() > draggedSize.height()) {
                        QnResourceWidget *draggedWidget = display()->widget(draggedWorkbenchItem);
                        QnResourceWidget *replacedWidget = display()->widget(replacedWorkbenchItem);

                        m_replacedWorkbenchItems.push_back(replacedWorkbenchItem);

                        if(draggedWidget->hasAspectRatio()) {
                            m_dragGeometries.push_back(bestDoubleBoundedGeometry(mapper(), replacedWorkbenchItem->geometry(), draggedWidget->aspectRatio()));
                        } else {
                            m_dragGeometries.push_back(replacedWorkbenchItem->geometry());
                        }

                        if(replacedWidget->hasAspectRatio()) {
                            m_dragGeometries.push_back(bestDoubleBoundedGeometry(mapper(), draggedWorkbenchItem->geometry(), replacedWidget->aspectRatio()));
                        } else {
                            m_dragGeometries.push_back(draggedWorkbenchItem->geometry());
                        }

                        finished = true;
                    }
                }
            }
        }

        /* Handle all other cases. */
        if(!finished) {
            QList<QRect> replacedGeometries;
            foreach (QnWorkbenchItem *workbenchItem, m_draggedWorkbenchItems) {
                QRect geometry = workbenchItem->geometry().adjusted(m_dragDelta.x(), m_dragDelta.y(), m_dragDelta.x(), m_dragDelta.y());
                m_dragGeometries.push_back(geometry);
                if(workbenchItem->isPinned())
                    replacedGeometries.push_back(geometry);
            }

            m_replacedWorkbenchItems = layout()->items(replacedGeometries).subtract(m_draggedWorkbenchItems.toSet()).toList();
            
            replacedGeometries.clear();
            foreach (QnWorkbenchItem *workbenchItem, m_replacedWorkbenchItems)
                replacedGeometries.push_back(workbenchItem->geometry().adjusted(-m_dragDelta.x(), -m_dragDelta.y(), -m_dragDelta.x(), -m_dragDelta.y()));

            m_dragGeometries.append(replacedGeometries);
            finished = true;
        }

        QnWorkbenchLayout::Disposition disposition;
        layout()->canMoveItems(m_draggedWorkbenchItems + m_replacedWorkbenchItems, m_dragGeometries, &disposition);

        m_display->gridItem()->setCellState(disposition.free, QnGridItem::ALLOWED);
        m_display->gridItem()->setCellState(disposition.occupied, QnGridItem::DISALLOWED);
    }
}

void QnWorkbenchController::at_dragFinished(QGraphicsView *, const QList<QGraphicsItem *> &) {
    qDebug("DRAGGING FINISHED");

    if(m_draggedWorkbenchItems.empty())
        return;

    /* Hide grid. */
    m_display->gridItem()->animatedHide();

    /* Do drag. */
    QList<QnWorkbenchItem *> workbenchItems = m_draggedWorkbenchItems + m_replacedWorkbenchItems;
    bool success = layout()->moveItems(workbenchItems, m_dragGeometries);

    foreach(QnWorkbenchItem *item, workbenchItems) {
        QnResourceWidget *widget = display()->widget(item);

        /* Adjust geometry deltas if everything went fine. */
        if(success)
            updateGeometryDelta(widget);

        /* Animate opacity back. */
        opacityAnimator(widget)->animateTo(1.0);
    }

    /* Re-sync everything. */
    foreach(QnWorkbenchItem *workbenchItem, workbenchItems)
        m_display->synchronize(workbenchItem);

    /* Un-raise the raised item if it was among the dragged ones. */
    QnWorkbenchItem *raisedItem = workbench()->item(QnWorkbench::RAISED);
    if(raisedItem != NULL && workbenchItems.contains(raisedItem))
        workbench()->setItem(QnWorkbench::RAISED, NULL);

    /* Clean up dragging state. */
    m_display->gridItem()->setCellState(m_dragGeometries, QnGridItem::INITIAL);
    m_dragDelta = invalidDragDelta();
    m_draggedItems.clear();
    m_draggedWorkbenchItems.clear();
    m_replacedWorkbenchItems.clear();
    m_dragGeometries.clear();
}

void QnWorkbenchController::at_rotationStarted(QGraphicsView *, QnResourceWidget *widget) {
    m_display->bringToFront(widget);
}

void QnWorkbenchController::at_rotationFinished(QGraphicsView *, QnResourceWidget *widget) {
    if(widget == NULL)
        return; /* We may get NULL if the widget being rotated gets deleted. */

    widget->item()->setRotation(widget->rotation());
}

void QnWorkbenchController::at_item_clicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info) {
    switch(info.button()) {
    case Qt::LeftButton:
        at_item_leftClicked(view, item, info);
        break;
    case Qt::RightButton:
        at_item_rightClicked(view, item, info);
        break;
    case Qt::MiddleButton:
        at_item_middleClicked(view, item, info);
        break;
    default:
        break;
    }
}

// TODO: remove function
void QnWorkbenchController::at_item_leftPressed(QGraphicsView *, QGraphicsItem *item, const ClickInfo &info) {
    if(info.modifiers() != 0)
        return;

    if(item->isSelected())
        return;

    QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
    if(widget == NULL)
        return;
}

void QnWorkbenchController::at_item_leftClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &info) {
    if(info.modifiers() != 0)
        return;

    if(workbench()->item(QnWorkbench::ZOOMED) != NULL)
        return; /* Don't change currently raised item if we're zoomed. It is surprising for the user. */

    QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
    if(widget == NULL)
        return;

    QnWorkbenchItem *workbenchItem = widget->item();

    workbench()->setItem(QnWorkbench::RAISED, workbench()->item(QnWorkbench::RAISED) == workbenchItem ? NULL : workbenchItem);
}

void QnWorkbenchController::at_item_rightClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &info) {
    QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
    if(widget == NULL)
        return;
    
    /* Right click does not select items. 
     * However, we need to select the item under mouse for the menu to work as expected. */
    if(!widget->isSelected()) {
        widget->scene()->clearSelection();
        widget->setSelected(true);
    }

    m_itemContextMenu->removeAction(m_hideMotionAction);
    m_itemContextMenu->removeAction(m_showMotionAction);

    int gridnowDisplayed = isMotionGridDisplayed();
    if (gridnowDisplayed == 1)
        m_itemContextMenu->addAction(m_hideMotionAction);
    else if (gridnowDisplayed == 0)
        m_itemContextMenu->addAction(m_showMotionAction);
    else {
        m_itemContextMenu->addAction(m_hideMotionAction);
        m_itemContextMenu->addAction(m_showMotionAction);
    }


    m_itemContextMenu->exec(info.screenPos());
}

void QnWorkbenchController::at_item_middleClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &) {
    QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
    if(widget == NULL)
        return;

    widget->item()->setRotation(0);
}

void QnWorkbenchController::at_item_doubleClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &) {
    QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
    if(widget == NULL)
        return;

    m_display->scene()->clearSelection();
    widget->setSelected(true);

    QnWorkbenchItem *workbenchItem = widget->item();
    QnWorkbenchItem *zoomedItem = workbench()->item(QnWorkbench::ZOOMED);
    if(zoomedItem == workbenchItem) {
        QRectF viewportGeometry = m_display->viewportGeometry();
        QRectF zoomedItemGeometry = m_display->itemGeometry(zoomedItem);

        if(contains(zoomedItemGeometry.size(), viewportGeometry.size()) && !qFuzzyCompare(viewportGeometry, zoomedItemGeometry)) {
            workbench()->setItem(QnWorkbench::ZOOMED, NULL);
            workbench()->setItem(QnWorkbench::ZOOMED, workbenchItem);
        } else {
            workbench()->setItem(QnWorkbench::ZOOMED, NULL);
        }
    } else {
        workbench()->setItem(QnWorkbench::ZOOMED, workbenchItem);
    }
}

void QnWorkbenchController::at_scene_clicked(QGraphicsView *view, const ClickInfo &info) {
    if(info.button() == Qt::LeftButton) {
        at_scene_leftClicked(view, info);
    } else {
        at_scene_rightClicked(view, info);
    }
}

void QnWorkbenchController::at_scene_leftClicked(QGraphicsView *, const ClickInfo &) {
    if(workbench() == NULL)
        return;

    workbench()->setItem(QnWorkbench::RAISED, NULL);
}

void QnWorkbenchController::at_scene_rightClicked(QGraphicsView *, const ClickInfo &info) {
    QScopedPointer<QMenu> menu(new QMenu(display()->view()));
    if(m_screenRecorder->isSupported()) {
        if(m_screenRecorder->isRecording() || (m_recordingAnimation && m_recordingAnimation->state() == QAbstractAnimation::Running)) {
            menu->addAction(m_stopRecordingAction);
        } else {
            menu->addAction(m_startRecordingAction);
        }
    }
    menu->addAction(m_recordingSettingsActions);

    menu->exec(info.screenPos());
}

void QnWorkbenchController::at_scene_doubleClicked(QGraphicsView *, const ClickInfo &) {
    if(workbench() == NULL)
        return;

    workbench()->setItem(QnWorkbench::ZOOMED, NULL);
    m_display->fitInView();
}

void QnWorkbenchController::at_viewportGrabbed() {
    qDebug("VIEWPORT GRABBED");
}

void QnWorkbenchController::at_viewportUngrabbed() {
    qDebug("VIEWPORT UNGRABBED");
}

void QnWorkbenchController::at_display_widgetChanged(QnWorkbench::ItemRole role) {
    QnResourceWidget *widget = m_display->widget(role);
    QnResourceWidget *oldWidget = m_widgetByRole[role];
    if(widget == oldWidget)
        return;

    m_widgetByRole[role] = widget;

    /* Update navigation item's target. */
    QnResourceWidget *navigatedWidget = m_widgetByRole[QnWorkbench::ZOOMED];
    if(navigatedWidget == NULL)
        navigatedWidget = m_widgetByRole[QnWorkbench::RAISED];
    m_navigationItem->setVideoCamera(widget == NULL ? NULL : navigatedWidget->display()->camera());

    switch(role) {
    case QnWorkbench::ZOOMED: {
        bool effective = widget == NULL;
        m_resizingInstrument->setEffective(effective);
        m_resizingInstrument->resizeHoverInstrument()->setEffective(effective);
        m_dragInstrument->setEffective(effective);
        break;
    }
    default:
        break;
    }
}

void QnWorkbenchController::at_display_widgetAdded(QnResourceWidget *widget) {
    if(widget->display() == NULL || widget->display()->camera() == NULL)
        return;

    QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>();
    if(cameraResource != NULL) 
    {
        connect(widget, SIGNAL(motionRegionSelected(QnResourcePtr, QRegion)), m_navigationItem, SLOT(loadMotionPeriods(QnResourcePtr, QRegion)));
        m_navigationItem->addReserveCamera(widget->display()->camera());
    }
}

void QnWorkbenchController::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(widget->display() == NULL || widget->display()->camera() == NULL)
        return;

    QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>();
    if(cameraResource != NULL)
        m_navigationItem->removeReserveCamera(widget->display()->camera());
}

void QnWorkbenchController::at_navigationItem_geometryChanged() {
    m_display->setViewportMargins(QMargins(
        0,
        0,
        0,
        std::floor(m_navigationItem->size().height())
    ));
}

void QnWorkbenchController::at_showMotionAction_triggered() 
{
    displayMotionGrid(display()->scene()->selectedItems(), true);
    m_motionSelectionInstrument->recursiveEnable();

#if 0
    
    CameraScheduleWidget* test1 = new CameraScheduleWidget(0);
    test1->show();
#endif
#if 0
    QnResourceWidget* widget = dynamic_cast<QnResourceWidget*> (display()->scene()->selectedItems()[0]);
    QnCameraMotionMaskWidget* test2 = new QnCameraMotionMaskWidget(widget->resource()->getUniqueId());
    test2->show();
#endif
}

void QnWorkbenchController::at_hideMotionAction_triggered() {
    displayMotionGrid(display()->scene()->selectedItems(), false);
    m_motionSelectionInstrument->recursiveDisable();
}

int QnWorkbenchController::isMotionGridDisplayed() 
{
    bool allDisplayed = true;
    bool allNonDisplayed = true;
    foreach(QGraphicsItem *item, display()->scene()->selectedItems()) 
    {
        QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
        if(widget == NULL)
            continue;
        allDisplayed &= widget->isMotionGridDisplayed();
        allNonDisplayed &= !widget->isMotionGridDisplayed();
    }
    if (allDisplayed)
        return 1;
    else if (allNonDisplayed)
        return 0;
    else
        return -1;
}

void QnWorkbenchController::displayMotionGrid(const QList<QGraphicsItem *> &items, bool display) {
    foreach(QGraphicsItem *item, items) {
        QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
        if(widget == NULL)
            continue;

        widget->setDisplayFlag(QnResourceWidget::DISPLAY_MOTION_GRID, display);
    }
}

void QnWorkbenchController::at_toggleRecordingAction_triggered() {
    if(m_screenRecorder->isRecording() || (m_recordingAnimation && m_recordingAnimation->state() == QAbstractAnimation::Running)) {
        at_stopRecordingAction_triggered(); 
    } else {
        at_startRecordingAction_triggered();
    }
}

void QnWorkbenchController::at_startRecordingAction_triggered() {
    if(m_screenRecorder->isRecording())
        return;

    if(!m_screenRecorder->isSupported())
        return;

    m_countdownCanceled = false;

    QGLWidget *widget = qobject_cast<QGLWidget *>(display()->view()->viewport());
    if(widget == NULL) {
        qnWarning("Viewport was expected to be a QGLWidget.");
        return;
    }

    QWidget *view = display()->view();

    if (m_recordingLabel == 0)
        m_recordingLabel = new QLabel(view);

    m_recordingLabel->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    m_recordingLabel->resize(200, 200);
    m_recordingLabel->move(view->mapToGlobal(QPoint(0, 0)) + toPoint(view->size() - m_recordingLabel->size()) / 2);

    m_recordingLabel->setMask(createRoundRegion(18, 18, m_recordingLabel->rect()));
    m_recordingLabel->setText(tr("Recording started"));
    m_recordingLabel->setAlignment(Qt::AlignCenter);
    m_recordingLabel->setStyleSheet(QLatin1String("QLabel { font-size:22px; border-width: 2px; border-style: inset; border-color: #535353; border-radius: 18px; background: #212150; color: #a6a6a6; selection-background-color: ltblue }"));
    m_recordingLabel->setFocusPolicy(Qt::NoFocus);
    m_recordingLabel->show();

    if (m_recordingAnimation == 0)
        m_recordingAnimation = new QPropertyAnimation(m_recordingLabel, "windowOpacity", m_recordingLabel);
    m_recordingAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_recordingAnimation->setDuration(3000);
    m_recordingAnimation->setStartValue(1.0);
    m_recordingAnimation->setEndValue(0.6);

    m_recordingAnimation->disconnect();
    connect(m_recordingAnimation, SIGNAL(finished()), this, SLOT(at_recordingAnimation_finished()));
    connect(m_recordingAnimation, SIGNAL(valueChanged(QVariant)), this, SLOT(at_recordingAnimation_valueChanged(QVariant)));
    m_recordingLabel->setText(tr("Ready"));
    m_recordingAnimation->start();
}

void QnWorkbenchController::at_recordingAnimation_finished()
{
    m_recordingLabel->hide();
    if (!m_countdownCanceled)
    {
        QGLWidget *widget = qobject_cast<QGLWidget *>(display()->view()->viewport());
        if(widget)
            m_screenRecorder->startRecording(widget);
    }
    m_countdownCanceled = false;
};

void QnWorkbenchController::at_recordingAnimation_valueChanged(QVariant value)
{
    static double TICKS = 3;

    QPropertyAnimation* animation = dynamic_cast<QPropertyAnimation*> (sender());
    if (!animation)
        return;

    double normValue = 1.0 - (double) animation->currentTime() / animation->duration();

    QLabel* label = dynamic_cast<QLabel*> (animation->targetObject());
    if (!label)
        return;

    if (m_countdownCanceled) {
        label->setText(tr("Cancelled"));
        return;
    }

    double d = normValue * (TICKS+1);
    if (d >= TICKS)
        return;
    int n = int (d) + 1;
    label->setText(QString::number(n));
}

void QnWorkbenchController::at_stopRecordingAction_triggered() {
    if(!m_screenRecorder->isRecording()) {
        m_countdownCanceled = true;
        return;
    }

    if(!m_screenRecorder->isSupported())
        return;

    m_screenRecorder->stopRecording();
}

void QnWorkbenchController::at_recordingSettingsActions_triggered() {
    QScopedPointer<PreferencesWindow> dialog(new PreferencesWindow(display()->view()));
    dialog->setCurrentTab(4);
    dialog->exec();
}

void QnWorkbenchController::at_screenRecorder_error(const QString &errorMessage) {
    QMessageBox::warning(display()->view(), tr("Warning"), tr("Can't start recording due to following error: %1").arg(errorMessage));
}

void QnWorkbenchController::at_screenRecorder_recordingStarted() {
}

void QnWorkbenchController::at_screenRecorder_recordingFinished(const QString &recordedFileName) {
    QString suggetion = QFileInfo(recordedFileName).fileName();
    if (suggetion.isEmpty())
        suggetion = tr("recorded_video");

    QSettings settings;
    settings.beginGroup(QLatin1String("videoRecording"));

    QString previousDir = settings.value(QLatin1String("previousDir")).toString();
    QString selectedFilter;
    while (1) {
        QString filePath = QFileDialog::getSaveFileName(
            display()->view(), 
            tr("Save Recording As..."),
            previousDir + QLatin1Char('/') + suggetion,
            tr("Transport Stream (*.ts)"),
            &selectedFilter,
            QFileDialog::DontUseNativeDialog
        );

        if (!filePath.isEmpty()) {
            if (!filePath.endsWith(QLatin1String(".ts"), Qt::CaseInsensitive))
                filePath += selectedFilter.mid(selectedFilter.indexOf(QLatin1Char('.')), 3);
            
            QFile::remove(filePath);
            if (!QFile::rename(recordedFileName, filePath)) {
                QString message = QObject::tr("Can't overwrite file '%1'. Please try another name.").arg(filePath);
                CL_LOG(cl_logWARNING) cl_log.log(message, cl_logWARNING);
                QMessageBox::warning(display()->view(), QObject::tr("Warning"), message, QMessageBox::Ok, QMessageBox::NoButton);
                continue;
            }

            QnResourcePtr res = QnResourceDirectoryBrowser::instance().checkFile(filePath);
            if (res)
                qnResPool->addResource(res);

            settings.setValue(QLatin1String("previousDir"), QFileInfo(filePath).absolutePath());
        } else {
            QFile::remove(recordedFileName);
        }
        break;
    }
    settings.endGroup();
}

