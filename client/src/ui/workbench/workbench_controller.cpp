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
#include <QGraphicsProxyWidget>

#include <utils/common/util.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delete_later.h>
#include <utils/common/toggle.h>

#include <core/resource/resource_directory_browser.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <camera/resource_display.h>
#include <camera/cam_display.h>

#include <ui/screen_recording/screen_recorder.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>

#include "ui/dialogs/sign_dialog.h" // TODO: move out.

#include <ui/animation/viewport_animator.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/wheel_zoom_instrument.h>
#include <ui/graphics/instruments/rubber_band_instrument.h>
#include <ui/graphics/instruments/move_instrument.h>
#include <ui/graphics/instruments/rotation_instrument.h>
#include <ui/graphics/instruments/click_instrument.h>
#include <ui/graphics/instruments/bounding_instrument.h>
#include <ui/graphics/instruments/stop_instrument.h>
#include <ui/graphics/instruments/stop_accepted_instrument.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <ui/graphics/instruments/transform_listener_instrument.h>
#include <ui/graphics/instruments/selection_fixup_instrument.h>
#include <ui/graphics/instruments/drop_instrument.h>
#include <ui/graphics/instruments/drag_instrument.h>
#include <ui/graphics/instruments/resizing_instrument.h>
#include <ui/graphics/instruments/resize_hover_instrument.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/instruments/animation_instrument.h>
#include <ui/graphics/instruments/selection_overlay_hack_instrument.h>
#include <ui/graphics/instruments/grid_adjustment_instrument.h>
#include <ui/graphics/instruments/ptz_instrument.h>

#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/grid/grid_item.h>

#include <ui/help/help_handler.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action_target_provider.h>

#include <file_processor.h>

#include "workbench_layout.h"
#include "workbench_item.h"
#include "workbench_grid_mapper.h"
#include "workbench_utility.h"
#include "workbench_context.h"
#include "workbench.h"
#include "workbench_display.h"
#include "workbench_access_controller.h"


//#define QN_WORKBENCH_CONTROLLER_DEBUG

#ifdef QN_WORKBENCH_CONTROLLER_DEBUG
#   define TRACE(...) qDebug() << __VA_ARGS__;
#else
#   define TRACE(...)
#endif

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

    QPoint invalidDragDelta() {
        return QPoint(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
    }

    QPoint invalidCursorPos() {
        return invalidDragDelta();
    }

    QPoint modulo(const QPoint &pos, const QRect &rect) {
        return QPoint(
            rect.left() + (pos.x() - rect.left() + rect.width())  % rect.width(),
            rect.top()  + (pos.y() - rect.top()  + rect.height()) % rect.height()
        );
    }

    int distance(int l, int h, int v) {
        if(l > h)
            return distance(h, l, v);

        if(v <= l) {
            return l - v;
        } else if(v >= h) {
            return v - h;
        } else {
            return 0;
        }
    }

    template<class T>
    struct IsInstanceOf {
        template<class Y>
        bool operator()(const Y *value) const {
            return dynamic_cast<const T *>(value) != NULL;
        }
    };

    /** Opacity of video items when they are dragged / resized. */
    const qreal widgetManipulationOpacity = 0.3;

} // anonymous namespace

QnWorkbenchController::QnWorkbenchController(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_manager(display()->instrumentManager()),
    m_cursorPos(invalidCursorPos()),
    m_resizedWidget(NULL),
    m_dragDelta(invalidDragDelta()),
    m_screenRecorder(0)
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

    Instrument::EventTypeSet widgetMouseEventTypes = Instrument::makeSet(QEvent::MouseButtonPress, QEvent::MouseButtonDblClick, QEvent::MouseMove, QEvent::MouseButtonRelease);
    Instrument::EventTypeSet mouseEventTypes = Instrument::makeSet(mouseEventTypeArray);
    Instrument::EventTypeSet wheelEventTypes = Instrument::makeSet(QEvent::GraphicsSceneWheel);
    Instrument::EventTypeSet dndEventTypes = Instrument::makeSet(QEvent::GraphicsSceneDragEnter, QEvent::GraphicsSceneDragMove, QEvent::GraphicsSceneDragLeave, QEvent::GraphicsSceneDrop);
    Instrument::EventTypeSet keyEventTypes = Instrument::makeSet(QEvent::KeyPress, QEvent::KeyRelease);

    /* Install and configure instruments. */
    m_itemLeftClickInstrument = new ClickInstrument(Qt::LeftButton, 300, Instrument::Item, this);
    m_itemRightClickInstrument = new ClickInstrument(Qt::RightButton, 0, Instrument::Item, this);
    ClickInstrument *itemMiddleClickInstrument = new ClickInstrument(Qt::MiddleButton, 0, Instrument::Item, this);
    ClickInstrument *sceneClickInstrument = new ClickInstrument(Qt::LeftButton | Qt::RightButton, 0, Instrument::Scene, this);
    m_handScrollInstrument = new HandScrollInstrument(this);
    m_wheelZoomInstrument = new WheelZoomInstrument(this);
    m_rubberBandInstrument = new RubberBandInstrument(this);
    m_rotationInstrument = new RotationInstrument(this);
    m_resizingInstrument = new ResizingInstrument(this);
    m_dropInstrument = new DropInstrument(false, display()->context(), this);
    m_dragInstrument = new DragInstrument(this);
    BoundingInstrument *boundingInstrument = display()->boundingInstrument();
    SelectionOverlayHackInstrument *selectionOverlayHackInstrument = display()->selectionOverlayHackInstrument();
    m_moveInstrument = new MoveInstrument(this);
    m_itemMouseForwardingInstrument = new ForwardingInstrument(Instrument::Item, mouseEventTypes, this);
    SelectionFixupInstrument *selectionFixupInstrument = new SelectionFixupInstrument(this);
    m_motionSelectionInstrument = new MotionSelectionInstrument(this);
    GridAdjustmentInstrument *gridAdjustmentInstrument = new GridAdjustmentInstrument(workbench(), this);
    SignalingInstrument *sceneKeySignalingInstrument = new SignalingInstrument(Instrument::Scene, Instrument::makeSet(QEvent::KeyPress), this);
    PtzInstrument *ptzInstrument = new PtzInstrument(this);

    gridAdjustmentInstrument->setSpeed(QSizeF(0.25 / 360.0, 0.25 / 360.0));
    gridAdjustmentInstrument->setMaxSpacing(QSizeF(0.5, 0.5));

    m_motionSelectionInstrument->setColor(MotionSelectionInstrument::Base, subColor(qnGlobals->mrsColor(), qnGlobals->selectionOpacityDelta()));
    m_motionSelectionInstrument->setColor(MotionSelectionInstrument::Border, subColor(qnGlobals->mrsColor(), qnGlobals->selectionBorderDelta()));
    m_motionSelectionInstrument->setSelectionModifiers(Qt::ShiftModifier);

    m_rubberBandInstrument->setRubberBandZValue(display()->layerZValue(Qn::EffectsLayer));
    m_rotationInstrument->setRotationItemZValue(display()->layerZValue(Qn::EffectsLayer));
    ptzInstrument->setPtzItemZValue(display()->layerZValue(Qn::EffectsLayer));
    m_resizingInstrument->setEffectiveDistance(8);

    m_moveInstrument->addItemCondition(new InstrumentItemConditionAdaptor<IsInstanceOf<QnResourceWidget> >());
    m_rotationInstrument->addItemCondition(new InstrumentItemConditionAdaptor<IsInstanceOf<QnResourceWidget> >());

    /* Item instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::Item, mouseEventTypes, this));
    m_manager->installInstrument(m_resizingInstrument->resizeHoverInstrument());
    m_manager->installInstrument(selectionFixupInstrument);
    m_manager->installInstrument(m_itemMouseForwardingInstrument);
    m_manager->installInstrument(selectionFixupInstrument->preForwardingInstrument());
    m_manager->installInstrument(m_itemLeftClickInstrument);
    m_manager->installInstrument(m_itemRightClickInstrument);
    m_manager->installInstrument(itemMiddleClickInstrument);

    /* Scene instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::Scene, dndEventTypes, this));
    m_manager->installInstrument(m_dropInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::Scene, dndEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::Scene, dndEventTypes, this));

    m_manager->installInstrument(new StopInstrument(Instrument::Scene, wheelEventTypes, this));
    m_manager->installInstrument(m_wheelZoomInstrument);
    m_manager->installInstrument(gridAdjustmentInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::Scene, wheelEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::Scene, wheelEventTypes, this));

    m_manager->installInstrument(new StopInstrument(Instrument::Scene, mouseEventTypes, this));
    m_manager->installInstrument(sceneClickInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::Scene, mouseEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::Scene, mouseEventTypes, this));
    m_manager->installInstrument(ptzInstrument);

    m_manager->installInstrument(new StopInstrument(Instrument::Scene, keyEventTypes, this));
    m_manager->installInstrument(sceneKeySignalingInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::Scene, keyEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::Scene, keyEventTypes, this));

    /* View/viewport instruments. */
    m_manager->installInstrument(m_rotationInstrument, InstallationMode::InstallAfter, display()->transformationListenerInstrument());
    m_manager->installInstrument(m_handScrollInstrument);
    m_manager->installInstrument(m_resizingInstrument);
    m_manager->installInstrument(m_moveInstrument);
    m_manager->installInstrument(m_dragInstrument);
    m_manager->installInstrument(m_rubberBandInstrument);
    m_manager->installInstrument(m_motionSelectionInstrument);

    display()->setLayer(m_dropInstrument->surface(), Qn::UiLayer);

    connect(m_itemLeftClickInstrument,  SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_leftClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(m_itemLeftClickInstrument,  SIGNAL(doubleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                 this,                           SLOT(at_item_doubleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(m_itemRightClickInstrument, SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_rightClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(itemMiddleClickInstrument,  SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_middleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(sceneClickInstrument,       SIGNAL(clicked(QGraphicsView *, const ClickInfo &)),                                        this,                           SLOT(at_scene_clicked(QGraphicsView *, const ClickInfo &)));
    connect(sceneClickInstrument,       SIGNAL(doubleClicked(QGraphicsView *, const ClickInfo &)),                                  this,                           SLOT(at_scene_doubleClicked(QGraphicsView *, const ClickInfo &)));
    connect(m_moveInstrument,           SIGNAL(moveStarted(QGraphicsView *, QList<QGraphicsItem *>)),                               this,                           SLOT(at_moveStarted(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_moveInstrument,           SIGNAL(move(QGraphicsView *, const QPointF &)),                                             this,                           SLOT(at_move(QGraphicsView *, const QPointF &)));
    connect(m_moveInstrument,           SIGNAL(moveFinished(QGraphicsView *, QList<QGraphicsItem *>)),                              this,                           SLOT(at_moveFinished(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_resizingInstrument,       SIGNAL(resizingStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),          this,                           SLOT(at_resizingStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)));
    connect(m_resizingInstrument,       SIGNAL(resizing(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),                 this,                           SLOT(at_resizing(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)));
    connect(m_resizingInstrument,       SIGNAL(resizingFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),         this,                           SLOT(at_resizingFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)));
    connect(m_rotationInstrument,       SIGNAL(rotationStarted(QGraphicsView *, QGraphicsWidget *)),                                this,                           SLOT(at_rotationStarted(QGraphicsView *, QGraphicsWidget *)));
    connect(m_rotationInstrument,       SIGNAL(rotationFinished(QGraphicsView *, QGraphicsWidget *)),                               this,                           SLOT(at_rotationFinished(QGraphicsView *, QGraphicsWidget *)));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 this,                           SLOT(at_motionSelectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)));
    connect(m_motionSelectionInstrument, SIGNAL(selectionStarted(QGraphicsView *, QnMediaResourceWidget *)),                        this,                           SLOT(at_motionSelectionStarted(QGraphicsView *, QnMediaResourceWidget *)));
    connect(m_motionSelectionInstrument, SIGNAL(motionRegionSelected(QGraphicsView *, QnMediaResourceWidget *, const QRect &)),     this,                           SLOT(at_motionRegionSelected(QGraphicsView *, QnMediaResourceWidget *, const QRect &)));
    connect(m_motionSelectionInstrument, SIGNAL(motionRegionCleared(QGraphicsView *, QnMediaResourceWidget *)),                     this,                           SLOT(at_motionRegionCleared(QGraphicsView *, QnMediaResourceWidget *)));
    connect(sceneKeySignalingInstrument, SIGNAL(activated(QGraphicsScene *, QEvent *)),                                             this,                           SLOT(at_scene_keyPressed(QGraphicsScene *, QEvent *)));

    connect(m_handScrollInstrument,     SIGNAL(scrollStarted(QGraphicsView *)),                                                     boundingInstrument,             SLOT(dontEnforcePosition(QGraphicsView *)));
    connect(m_handScrollInstrument,     SIGNAL(scrollFinished(QGraphicsView *)),                                                    boundingInstrument,             SLOT(enforcePosition(QGraphicsView *)));

    connect(display(),                  SIGNAL(viewportGrabbed()),                                                                  m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(display(),                  SIGNAL(viewportUngrabbed()),                                                                m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(display(),                  SIGNAL(viewportGrabbed()),                                                                  m_wheelZoomInstrument,          SLOT(recursiveDisable()));
    connect(display(),                  SIGNAL(viewportUngrabbed()),                                                                m_wheelZoomInstrument,          SLOT(recursiveEnable()));

    connect(m_handScrollInstrument,     SIGNAL(scrollProcessStarted(QGraphicsView *)),                                              m_itemMouseForwardingInstrument, SLOT(recursiveDisable()));
    connect(m_handScrollInstrument,     SIGNAL(scrollProcessFinished(QGraphicsView *)),                                             m_itemMouseForwardingInstrument, SLOT(recursiveEnable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandProcessStarted(QGraphicsView *)),                                          m_itemMouseForwardingInstrument, SLOT(recursiveDisable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandProcessFinished(QGraphicsView *)),                                         m_itemMouseForwardingInstrument, SLOT(recursiveEnable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 m_itemMouseForwardingInstrument, SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessFinished(QGraphicsView *, QnMediaResourceWidget *)),                m_itemMouseForwardingInstrument, SLOT(recursiveEnable()));

    connect(m_moveInstrument,           SIGNAL(moveStarted(QGraphicsView *, QList<QGraphicsItem *>)),                               selectionOverlayHackInstrument, SLOT(recursiveDisable()));
    connect(m_moveInstrument,           SIGNAL(moveFinished(QGraphicsView *, QList<QGraphicsItem *>)),                              selectionOverlayHackInstrument, SLOT(recursiveEnable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandStarted(QGraphicsView *)),                                                 selectionOverlayHackInstrument, SLOT(recursiveDisable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandFinished(QGraphicsView *)),                                                selectionOverlayHackInstrument, SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),          selectionOverlayHackInstrument, SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),         selectionOverlayHackInstrument, SLOT(recursiveEnable()));

    connect(m_dragInstrument,           SIGNAL(dragProcessStarted(QGraphicsView *)),                                                m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_dragInstrument,           SIGNAL(dragProcessFinished(QGraphicsView *)),                                               m_moveInstrument,               SLOT(recursiveEnable()));

    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),   m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),  m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),   m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),  m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),   m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, const ResizingInfo &)),  m_rubberBandInstrument,         SLOT(recursiveEnable()));

    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        m_rubberBandInstrument,         SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         m_resizingInstrument,           SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        m_resizingInstrument,           SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QGraphicsWidget *)),                         ptzInstrument,                  SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QGraphicsWidget *)),                        ptzInstrument,                  SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationStarted(QGraphicsView *, QGraphicsWidget *)),                                m_motionSelectionInstrument,    SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationFinished(QGraphicsView *, QGraphicsWidget *)),                               m_motionSelectionInstrument,    SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationStarted(QGraphicsView *, QGraphicsWidget *)),                                boundingInstrument,             SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationFinished(QGraphicsView *, QGraphicsWidget *)),                               boundingInstrument,             SLOT(recursiveEnable()));

    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessFinished(QGraphicsView *, QnMediaResourceWidget *)),                m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessFinished(QGraphicsView *, QnMediaResourceWidget *)),                m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessFinished(QGraphicsView *, QnMediaResourceWidget *)),                m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 m_resizingInstrument,           SLOT(recursiveDisable()));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessFinished(QGraphicsView *, QnMediaResourceWidget *)),                m_resizingInstrument,           SLOT(recursiveEnable()));

    connect(ptzInstrument,              SIGNAL(ptzProcessStarted(QnMediaResourceWidget *)),                                         m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessFinished(QnMediaResourceWidget *)),                                        m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessStarted(QnMediaResourceWidget *)),                                         m_moveInstrument,               SLOT(recursiveDisable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessFinished(QnMediaResourceWidget *)),                                        m_moveInstrument,               SLOT(recursiveEnable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessStarted(QnMediaResourceWidget *)),                                         m_resizingInstrument,           SLOT(recursiveDisable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessFinished(QnMediaResourceWidget *)),                                        m_resizingInstrument,           SLOT(recursiveEnable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessStarted(QnMediaResourceWidget *)),                                         m_motionSelectionInstrument,    SLOT(recursiveDisable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessFinished(QnMediaResourceWidget *)),                                        m_motionSelectionInstrument,    SLOT(recursiveEnable()));

    /* Connect to display. */
    connect(display(),                  SIGNAL(widgetChanged(Qn::ItemRole)),                                                        this,                           SLOT(at_display_widgetChanged(Qn::ItemRole)));
    connect(display(),                  SIGNAL(widgetAdded(QnResourceWidget *)),                                                    this,                           SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display(),                  SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)),                                         this,                           SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(workbench(),                SIGNAL(currentLayoutChanged()),                                                             this,                           SLOT(at_workbench_currentLayoutChanged()));

    /* Set up zoom toggle. */
    m_zoomedToggle = new QnToggle(false, this);
    connect(m_zoomedToggle,             SIGNAL(activated()),                                                                        m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_zoomedToggle,             SIGNAL(deactivated()),                                                                      m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_zoomedToggle,             SIGNAL(activated()),                                                                        m_resizingInstrument,           SLOT(recursiveDisable()));
    connect(m_zoomedToggle,             SIGNAL(deactivated()),                                                                      m_resizingInstrument,           SLOT(recursiveEnable()));
    connect(m_zoomedToggle,             SIGNAL(activated()),                                                                        m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_zoomedToggle,             SIGNAL(deactivated()),                                                                      m_rubberBandInstrument,         SLOT(recursiveEnable()));
    connect(m_zoomedToggle,             SIGNAL(activated()),                                                                        this,                           SLOT(at_zoomedToggle_activated()));
    connect(m_zoomedToggle,             SIGNAL(deactivated()),                                                                      this,                           SLOT(at_zoomedToggle_deactivated()));
    m_zoomedToggle->setActive(display()->widget(Qn::ZoomedRole) != NULL);

    /* Set up context menu. */
    QWidget *window = display()->view()->window();
    if (QnScreenRecorder::isSupported())
        window->addAction(action(Qn::ToggleScreenRecordingAction));
    window->addAction(action(Qn::ToggleSmartSearchAction));
    window->addAction(action(Qn::ToggleInfoAction));

    connect(action(Qn::SelectAllAction), SIGNAL(triggered()),                                                                       this,                           SLOT(at_selectAllAction_triggered()));
    connect(action(Qn::StartSmartSearchAction), SIGNAL(triggered()),                                                                this,                           SLOT(at_startSmartSearchAction_triggered()));
    connect(action(Qn::StopSmartSearchAction), SIGNAL(triggered()),                                                                 this,                           SLOT(at_stopSmartSearchAction_triggered()));
    connect(action(Qn::ToggleSmartSearchAction), SIGNAL(triggered()),                                                               this,                           SLOT(at_toggleSmartSearchAction_triggered()));
    connect(action(Qn::ClearMotionSelectionAction), SIGNAL(triggered()),                                                            this,                           SLOT(at_clearMotionSelectionAction_triggered()));
    connect(action(Qn::ShowInfoAction), SIGNAL(triggered()),                                                                        this,                           SLOT(at_showInfoAction_triggered()));
    connect(action(Qn::HideInfoAction), SIGNAL(triggered()),                                                                        this,                           SLOT(at_hideInfoAction_triggered()));
    connect(action(Qn::ToggleInfoAction), SIGNAL(triggered()),                                                                      this,                           SLOT(at_toggleInfoAction_triggered()));
    connect(action(Qn::CheckFileSignatureAction), SIGNAL(triggered()),                                                              this,                           SLOT(at_checkFileSignatureAction_triggered()));
    connect(action(Qn::MaximizeItemAction), SIGNAL(triggered()),                                                                    this,                           SLOT(at_maximizeItemAction_triggered()));
    connect(action(Qn::UnmaximizeItemAction), SIGNAL(triggered()),                                                                  this,                           SLOT(at_unmaximizeItemAction_triggered()));
    if (QnScreenRecorder::isSupported())
        connect(action(Qn::ToggleScreenRecordingAction), SIGNAL(triggered(bool)),                                                   this,                           SLOT(at_recordingAction_triggered(bool)));
    connect(action(Qn::FitInViewAction), SIGNAL(triggered()),                                                                       this,                           SLOT(at_fitInViewAction_triggered()));

    /* Init screen recorder. */
    if (QnScreenRecorder::isSupported()){
        m_screenRecorder = new QnScreenRecorder(this);
        connect(m_screenRecorder,       SIGNAL(recordingStarted()),                                                                 this,                           SLOT(at_screenRecorder_recordingStarted()));
        connect(m_screenRecorder,       SIGNAL(recordingFinished(QString)),                                                         this,                           SLOT(at_screenRecorder_recordingFinished(QString)));
        connect(m_screenRecorder,       SIGNAL(error(QString)),                                                                     this,                           SLOT(at_screenRecorder_error(QString)));
    }
    m_countdownCanceled = false;
    m_recordingLabel = 0;
    m_recordingAnimation = 0;
}

QnWorkbenchController::~QnWorkbenchController() {
    if (m_screenRecorder){
        disconnect(m_screenRecorder, NULL, this, NULL);
        m_screenRecorder->stopRecording();
    }
}

QnWorkbenchGridMapper *QnWorkbenchController::mapper() const {
    return workbench()->mapper();
}

bool QnWorkbenchController::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Close) {
        if (QnResourceWidget *widget = qobject_cast<QnResourceWidget *>(watched)) {
            /* Clicking on close button of a widget that is not selected should clear selection. */
            if(!widget->isSelected())
                display()->scene()->clearSelection();

            menu()->trigger(Qn::RemoveLayoutItemAction, widget);
            event->ignore();
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
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

void QnWorkbenchController::displayMotionGrid(const QList<QnResourceWidget *> &widgets, bool display) {
    foreach(QnResourceWidget *widget, widgets) {
        if(!(widget->resource()->flags() & QnResource::motion))
            continue;

        widget->setOption(QnResourceWidget::DisplayMotion, display);
    }
}

void QnWorkbenchController::displayWidgetInfo(const QList<QnResourceWidget *> &widgets, bool display){
    foreach(QnResourceWidget *widget, widgets)
        widget->setOption(QnResourceWidget::DisplayInfo, display);
}

void QnWorkbenchController::moveCursor(const QPoint &direction) {
    QnWorkbenchItem *centerItem = m_cursorItem.data();
    if(!centerItem)
        centerItem = workbench()->currentLayout()->item(m_cursorPos);

    QPoint center = m_cursorPos;
    if(centerItem && !centerItem->geometry().contains(center))
        center = centerItem->geometry().topLeft();

    QRect centerRect;
    if(centerItem) {
        centerRect = centerItem->geometry();
    } else {
        centerRect = QRect(center, QSize(1, 1));
    }
    
    QPoint aAxis = direction; /* Direction axis (a-axis) */
    QPoint bAxis = QPoint(-aAxis.y(), aAxis.x()); /* Axis perpendicular to direction (b-axis). */

    int aCenterRectL = dot(centerRect.topLeft(), aAxis);
    int aCenterRectH = dot(centerRect.bottomRight(), aAxis);
    int bCenterRectL = dot(centerRect.topLeft(), bAxis);
    int bCenterRectH = dot(centerRect.bottomRight(), bAxis);

    QRect boundingRect = workbench()->currentLayout()->boundingRect();
    int aBoundingSize = qAbs(dot(toPoint(boundingRect.size()), aAxis)); /* Size of the bounding rect projected to the a-axis. */

    int bestDistance = std::numeric_limits<int>::max();
    QPoint bestPos = center;

    foreach(QnWorkbenchItem *item, workbench()->currentLayout()->items()) {
        if(item == centerItem)
            continue;

        const QRect &geometry = item->geometry();
        for (int r = geometry.top(); r <= geometry.bottom(); r++) {
            for (int c = geometry.left(); c <= geometry.right(); c++) {
                QPoint pos(c, r);

                int arDistance = distance(aCenterRectL, aCenterRectH, dot(pos, aAxis));
                int brDistance = distance(bCenterRectL, bCenterRectH, dot(pos, bAxis));
                if(brDistance > arDistance)
                    continue;

                int aDistance = (dot(pos - center, aAxis) + aBoundingSize) % aBoundingSize;
                int bDistance = qAbs(dot(pos - center, bAxis));

                int distance = (aDistance << 16) + bDistance;
                if(distance < bestDistance) {
                    bestDistance = distance;
                    bestPos = pos;
                }
            }
        }
    }

    if(bestPos == center)
        return;

    Qn::ItemRole role = Qn::ZoomedRole;
    if(!workbench()->item(role))
        role = Qn::RaisedRole;

    QnWorkbenchItem *bestItem = workbench()->currentLayout()->item(bestPos);
    display()->scene()->clearSelection();
    display()->widget(bestItem)->setSelected(true);
    workbench()->setItem(role, bestItem);
    m_cursorPos = bestPos;
    m_cursorItem = bestItem;
}

void QnWorkbenchController::showContextMenuAt(const QPoint &pos){
    QScopedPointer<QMenu> menu(this->menu()->newMenu(Qn::SceneScope, display()->scene()->selectedItems()));
    if(menu->isEmpty())
        return;

    menu->exec(pos);
}


// -------------------------------------------------------------------------- //
// Screen recording
// -------------------------------------------------------------------------- //
void QnWorkbenchController::startRecording()
{
    if (!m_screenRecorder) {
        return;
    }

    action(Qn::ToggleScreenRecordingAction)->setChecked(true);

    if(m_screenRecorder->isRecording() || (m_recordingAnimation && m_recordingAnimation->state() == QAbstractAnimation::Running))
        return;

    m_countdownCanceled = false;

    QGLWidget *widget = qobject_cast<QGLWidget *>(display()->view()->viewport());
    if (widget == NULL) {
        qnWarning("Viewport was expected to be a QGLWidget.");
        return;
    }

    QWidget *view = display()->view();

    if (m_recordingLabel == 0)
        m_recordingLabel = new QLabel(view);
    m_recordingLabel->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    m_recordingLabel->resize(220, 165);
    m_recordingLabel->move(view->mapToGlobal(QPoint(0, 0)) + toPoint(view->size() - m_recordingLabel->size()) / 2);

    m_recordingLabel->setMask(createRoundRegion(18, 18, m_recordingLabel->rect()));
    m_recordingLabel->setText(tr("Recording in..."));
    m_recordingLabel->setAlignment(Qt::AlignCenter);
    m_recordingLabel->setStyleSheet(QLatin1String("QLabel { font-size:22px; border-width: 2px; border-style: inset; border-color: #535353; border-radius: 18px; background: #212150; color: #a6a6a6; selection-background-color: lightblue }"));
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
    m_recordingAnimation->start();
}

void QnWorkbenchController::stopRecording()
{
    if (!m_screenRecorder)
        return;

    action(Qn::ToggleScreenRecordingAction)->setChecked(false);

    if (!m_screenRecorder->isRecording()) {
        m_countdownCanceled = true;
        return;
    }

    m_screenRecorder->stopRecording();
}

void QnWorkbenchController::at_recordingAnimation_finished()
{
    m_recordingLabel->hide();
    if (!m_countdownCanceled) {
        if (QGLWidget *widget = qobject_cast<QGLWidget *>(display()->view()->viewport()))
            if (m_screenRecorder) // just in case =)
                m_screenRecorder->startRecording(widget);
    }
    m_countdownCanceled = false;
}

void QnWorkbenchController::at_recordingAnimation_valueChanged(const QVariant &)
{
    static double TICKS = 3;

    QPropertyAnimation *animation = qobject_cast<QPropertyAnimation *>(sender());
    if (!animation)
        return;

    double normValue = 1.0 - (double) animation->currentTime() / animation->duration();

    QLabel *label = qobject_cast<QLabel *>(animation->targetObject());
    if (!label)
        return;

    if (m_countdownCanceled) {
        label->setText(tr("Cancelled"));
        return;
    }

    double d = normValue * (TICKS+1);
    if (d < TICKS) {
        const int n = int (d) + 1;
        label->setText(tr("Recording in...") + QString::number(n));
    }
}

void QnWorkbenchController::at_screenRecorder_recordingStarted() {
    return;
}

void QnWorkbenchController::at_screenRecorder_error(const QString &errorMessage) {
    if (QnScreenRecorder::isSupported())
        action(Qn::ToggleScreenRecordingAction)->setChecked(false);

    QMessageBox::warning(display()->view(), tr("Warning"), tr("Can't start recording due to the following error: %1").arg(errorMessage));
}

void QnWorkbenchController::at_screenRecorder_recordingFinished(const QString &recordedFileName) {
    QString suggetion = QFileInfo(recordedFileName).fileName();
    if (suggetion.isEmpty())
        suggetion = tr("recorded_video");

    QSettings settings;
    settings.beginGroup(QLatin1String("videoRecording"));

    QString previousDir = settings.value(QLatin1String("previousDir")).toString();
    QString selectedFilter;
    while (true) {
        QString filePath = QFileDialog::getSaveFileName(
            display()->view(),
            tr("Save Recording As..."),
            previousDir + QLatin1Char('/') + suggetion,
            tr("AVI (Audio/Video Interleaved) (*.avi)"),
            &selectedFilter,
            QFileDialog::DontUseNativeDialog
        );

        if (!filePath.isEmpty()) {
            if (!filePath.endsWith(QLatin1String(".avi"), Qt::CaseInsensitive))
                filePath += selectedFilter.mid(selectedFilter.indexOf(QLatin1Char('.')), 4);

            QFile::remove(filePath);
            if (!QFile::rename(recordedFileName, filePath)) {
                QString message = QObject::tr("Can't overwrite file '%1'. Please try another name.").arg(filePath);
                CL_LOG(cl_logWARNING) cl_log.log(message, cl_logWARNING);
                QMessageBox::warning(display()->view(), QObject::tr("Warning"), message, QMessageBox::Ok, QMessageBox::NoButton);
                continue;
            }

            QnFileProcessor::createResourcesForFile(filePath);

            settings.setValue(QLatin1String("previousDir"), QFileInfo(filePath).absolutePath());
        } else {
            QFile::remove(recordedFileName);
        }
        break;
    }
    settings.endGroup();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchController::at_scene_keyPressed(QGraphicsScene *, QEvent *event) {
    if(event->type() != QEvent::KeyPress)
        return;

    event->accept(); /* Accept by default. */

    QKeyEvent *e = static_cast<QKeyEvent *>(event);
    switch(e->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return: {
        QList<QGraphicsItem *> items = display()->scene()->selectedItems();
        if(items.size() == 1 && dynamic_cast<QnResourceWidget *>(items[0])) {
            if(items[0] == display()->widget(Qn::ZoomedRole)) {
                menu()->trigger(Qn::UnmaximizeItemAction, items);
            } else {
                menu()->trigger(Qn::MaximizeItemAction, items);
            }
        }
        return;
    }
    case Qt::Key_Up:
        if(e->modifiers() == 0)
            moveCursor(QPoint(0, -1));
        if(e->modifiers() & Qt::AltModifier)
            m_handScrollInstrument->emulate(QPoint(0, -15));
        return;
    case Qt::Key_Down:
        if(e->modifiers() == 0)
            moveCursor(QPoint(0, 1));
        if(e->modifiers() & Qt::AltModifier)
            m_handScrollInstrument->emulate(QPoint(0, 15));
        return;
    case Qt::Key_Left:
        if(e->modifiers() == 0)
            moveCursor(QPoint(-1, 0));
        if(e->modifiers() & Qt::AltModifier)
            m_handScrollInstrument->emulate(QPoint(-15, 0));
        return;
    case Qt::Key_Right:
        if(e->modifiers() == 0)
            moveCursor(QPoint(1, 0));
        if(e->modifiers() & Qt::AltModifier)
            m_handScrollInstrument->emulate(QPoint(15, 0));
        return;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        m_wheelZoomInstrument->emulate(30);
        return;
    case Qt::Key_Minus:
        m_wheelZoomInstrument->emulate(-30);
        return;
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
        break; /* Don't let the view handle these and scroll. */
    case Qt::Key_Menu:{
        QGraphicsView *view = display()->view();        
        QList<QGraphicsItem *> items = display()->scene()->selectedItems();
        QPoint offset = view->mapToGlobal(QPoint(0, 0));
        if (items.count() == 0)
            showContextMenuAt(offset);
        else{ 
            QRectF rect = items[0]->mapToScene(items[0]->boundingRect()).boundingRect();
            QRect testRect = QnSceneTransformations::mapRectFromScene(view, rect); /* Where is the static analogue? */ 
            showContextMenuAt(offset + testRect.bottomRight());
        }
        }
        break;
    default:
        event->ignore(); /* Wasn't recognized? Ignore. */
        break;
    }
}

void QnWorkbenchController::at_resizingStarted(QGraphicsView *, QGraphicsWidget *item, const ResizingInfo &) {
    TRACE("RESIZING STARTED");

    m_resizedWidget = qobject_cast<QnResourceWidget *>(item);
    if(m_resizedWidget == NULL)
        return;

    workbench()->setItem(Qn::RaisedRole, NULL); /* Un-raise currently raised item so that it doesn't interfere with resizing. */

    display()->bringToFront(m_resizedWidget);
    display()->gridItem()->animatedShow();
    opacityAnimator(m_resizedWidget)->animateTo(widgetManipulationOpacity);
}

void QnWorkbenchController::at_resizing(QGraphicsView *, QGraphicsWidget *item, const ResizingInfo &info) {
    Q_UNUSED(info)
    if(m_resizedWidget != item || item == NULL)
        return;

    QnResourceWidget *widget = m_resizedWidget;
    QRectF gridRectF = mapper()->mapToGridF(widget->geometry());
    
    /* Calculate integer size. */
    QSizeF gridSizeF = gridRectF.size();
    QSize gridSize = mapper()->mapToGrid(widget->size());
    if(widget->hasAspectRatio()) {
        if(widget->aspectRatio() > 1.0) {
            gridSize = bestSingleBoundedSize(mapper(), gridSize.width(), Qt::Horizontal, widget->aspectRatio());
        } else {
            gridSize = bestSingleBoundedSize(mapper(), gridSize.height(), Qt::Vertical, widget->aspectRatio());
        }
    }

    /* Calculate integer position. */
    QPointF gridPosF = gridRectF.topLeft() + toPoint(gridSizeF - gridSize) / 2.0;
    QPoint gridPos = gridPosF.toPoint(); /* QPointF::toPoint() uses qRound() internally. */

    /* Calculate new grid rect based on the dragged frame section. */
    QRect newResizingWidgetRect = QRect(gridPos, gridSize);
    if(newResizingWidgetRect != m_resizedWidgetRect) {
        QnWorkbenchLayout::Disposition disposition;
        widget->item()->layout()->canMoveItem(widget->item(), newResizingWidgetRect, &disposition);

        display()->gridItem()->setCellState(m_resizedWidgetRect, QnGridItem::INITIAL);
        display()->gridItem()->setCellState(disposition.free, QnGridItem::ALLOWED);
        display()->gridItem()->setCellState(disposition.occupied, QnGridItem::DISALLOWED);

        m_resizedWidgetRect = newResizingWidgetRect;
    }
}

void QnWorkbenchController::at_resizingFinished(QGraphicsView *, QGraphicsWidget *item, const ResizingInfo &) {
    TRACE("RESIZING FINISHED");

    display()->gridItem()->animatedHide();
    opacityAnimator(m_resizedWidget)->animateTo(1.0);

    if(m_resizedWidget == item && item != NULL) {
        QnResourceWidget *widget = m_resizedWidget;

        /* Resize if possible. */
        QSet<QnWorkbenchItem *> entities = widget->item()->layout()->items(m_resizedWidgetRect);
        entities.remove(widget->item());
        if (entities.empty()) {
            widget->item()->setGeometry(m_resizedWidgetRect);
            updateGeometryDelta(widget);
        }

        display()->synchronize(widget->item());

        /* Un-raise the raised item if it was the one being resized. */
        QnWorkbenchItem *raisedItem = workbench()->item(Qn::RaisedRole);
        if(raisedItem == widget->item())
            workbench()->setItem(Qn::RaisedRole, NULL);
    }

    /* Clean up resizing state. */
    display()->gridItem()->setCellState(m_resizedWidgetRect, QnGridItem::INITIAL);
    m_resizedWidgetRect = QRect();
    m_resizedWidget = NULL;
}

void QnWorkbenchController::at_moveStarted(QGraphicsView *, const QList<QGraphicsItem *> &items) {
    TRACE("MOVE STARTED");

    /* Build item lists. */
    foreach (QGraphicsItem *item, items) {
        QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
        if(widget == NULL)
            continue;

        m_draggedWorkbenchItems.push_back(widget->item());

        opacityAnimator(widget)->animateTo(widgetManipulationOpacity);
    }
    if(m_draggedWorkbenchItems.empty())
        return;

    /* Bring to front preserving relative order. */
    display()->bringToFront(items);
    display()->setLayer(items, Qn::FrontLayer);

    /* Show grid. */
    display()->gridItem()->animatedShow();
}

void QnWorkbenchController::at_move(QGraphicsView *, const QPointF &totalDelta) {
    if(m_draggedWorkbenchItems.empty())
        return;

    QnWorkbenchLayout *layout = m_draggedWorkbenchItems[0]->layout();

    QPoint newDragDelta = mapper()->mapDeltaToGridF(totalDelta).toPoint();
    if(newDragDelta != m_dragDelta) {
        display()->gridItem()->setCellState(m_dragGeometries, QnGridItem::INITIAL);

        m_dragDelta = newDragDelta;
        m_replacedWorkbenchItems.clear();
        m_dragGeometries.clear();

        /* Handle single widget case. */
        bool finished = false;
        if(m_draggedWorkbenchItems.size() == 1) {
            QnWorkbenchItem *draggedWorkbenchItem = m_draggedWorkbenchItems[0];

            /* Find item that dragged item was dropped on. */
            QSet<QnWorkbenchItem *> replacedWorkbenchItems = layout->items(draggedWorkbenchItem->geometry().adjusted(m_dragDelta.x(), m_dragDelta.y(), m_dragDelta.x(), m_dragDelta.y()));
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

            m_replacedWorkbenchItems = layout->items(replacedGeometries).subtract(m_draggedWorkbenchItems.toSet()).toList();

            replacedGeometries.clear();
            foreach (QnWorkbenchItem *workbenchItem, m_replacedWorkbenchItems)
                replacedGeometries.push_back(workbenchItem->geometry().adjusted(-m_dragDelta.x(), -m_dragDelta.y(), -m_dragDelta.x(), -m_dragDelta.y()));

            m_dragGeometries.append(replacedGeometries);
            finished = true;
        }

        QnWorkbenchLayout::Disposition disposition;
        layout->canMoveItems(m_draggedWorkbenchItems + m_replacedWorkbenchItems, m_dragGeometries, &disposition);

        display()->gridItem()->setCellState(disposition.free, QnGridItem::ALLOWED);
        display()->gridItem()->setCellState(disposition.occupied, QnGridItem::DISALLOWED);
    }
}

void QnWorkbenchController::at_moveFinished(QGraphicsView *, const QList<QGraphicsItem *> &) {
    TRACE("MOVE FINISHED");

    /* Hide grid. */
    display()->gridItem()->animatedHide();

    if(!m_draggedWorkbenchItems.empty()) {
        QnWorkbenchLayout *layout = m_draggedWorkbenchItems[0]->layout();

        /* Do drag. */
        QList<QnWorkbenchItem *> workbenchItems = m_draggedWorkbenchItems + m_replacedWorkbenchItems;
        bool success = layout->moveItems(workbenchItems, m_dragGeometries);

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
            display()->synchronize(workbenchItem);

        /* Un-raise the raised item if it was among the dragged ones. */
        QnWorkbenchItem *raisedItem = workbench()->item(Qn::RaisedRole);
        if(raisedItem != NULL && workbenchItems.contains(raisedItem))
            workbench()->setItem(Qn::RaisedRole, NULL);
    }

    /* Clean up dragging state. */
    display()->gridItem()->setCellState(m_dragGeometries, QnGridItem::INITIAL);
    m_dragDelta = invalidDragDelta();
    m_draggedWorkbenchItems.clear();
    m_replacedWorkbenchItems.clear();
    m_dragGeometries.clear();
}

void QnWorkbenchController::at_rotationStarted(QGraphicsView *, QGraphicsWidget *widget) {
    TRACE("ROTATION STARTED");

    display()->bringToFront(widget);
}

void QnWorkbenchController::at_rotationFinished(QGraphicsView *, QGraphicsWidget *widget) {
    TRACE("ROTATION FINISHED");

    QnResourceWidget *resourceWidget = dynamic_cast<QnResourceWidget *>(widget);
    if(!resourceWidget)
        return; /* We may also get NULL if the widget being rotated gets deleted. */

    resourceWidget->item()->setRotation(widget->rotation());
}

void QnWorkbenchController::at_motionSelectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *widget) {
    if(!(widget->resource()->flags() & QnResource::motion)) { // TODO: move to instrument item condition
        m_motionSelectionInstrument->resetLater();
        return;
    }

    widget->setOption(QnResourceWidget::DisplayMotion, true);
}

void QnWorkbenchController::at_motionSelectionStarted(QGraphicsView *, QnMediaResourceWidget *widget) {
    foreach(QnResourceWidget *otherWidget, display()->widgets())
        if(otherWidget != widget)
            if(QnMediaResourceWidget *otherMediaWidget = dynamic_cast<QnMediaResourceWidget *>(otherWidget))
                otherMediaWidget->clearMotionSelection();
}

void QnWorkbenchController::at_motionRegionCleared(QGraphicsView *, QnMediaResourceWidget *widget) {
    if(widget->resource()->flags() & QnResource::motion) // TODO: #Elric maybe there is a better place for this check?
        widget->clearMotionSelection();
}

void QnWorkbenchController::at_motionRegionSelected(QGraphicsView *, QnMediaResourceWidget *widget, const QRect &region) {
    if(widget->resource()->flags() & QnResource::motion) // TODO: #Elric maybe there is a better place for this check?
        widget->addToMotionSelection(region);
}

void QnWorkbenchController::at_item_leftClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &info) {
    TRACE("ITEM LCLICKED");

    if(info.modifiers() != 0)
        return;

    if(workbench()->item(Qn::ZoomedRole) != NULL)
        return; /* Don't change currently raised item if we're zoomed. It is surprising for the user. */

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return;

    if(widget->options() & QnResourceWidget::ControlPtz)
        return; /* (Un)raising shouldn't work when PTZ is on as it's confusing. */

    QnWorkbenchItem *workbenchItem = widget->item();

    workbench()->setItem(Qn::RaisedRole, workbench()->item(Qn::RaisedRole) == workbenchItem ? NULL : workbenchItem);
}

void QnWorkbenchController::at_item_rightClicked(QGraphicsView *view, QGraphicsItem *item, const ClickInfo &info) {
    Q_UNUSED(view)
    TRACE("ITEM RCLICKED");

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return;

    /* Right click does not select items.
     * However, we need to select the item under mouse for the menu to work as expected. */
    if(!widget->isSelected()) {
        widget->scene()->clearSelection();
        widget->setSelected(true);
    }
    showContextMenuAt(info.screenPos());
}

void QnWorkbenchController::at_item_middleClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &) {
    TRACE("ITEM MCLICKED");

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return;

    widget->item()->setRotation(0);
}

void QnWorkbenchController::at_item_doubleClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &) {
    TRACE("ITEM DOUBLECLICKED");

    QnResourceWidget *widget = item->isWidget() ? qobject_cast<QnResourceWidget *>(item->toGraphicsObject()) : NULL;
    if(widget == NULL)
        return;

    display()->scene()->clearSelection();
    widget->setSelected(true);

    QnWorkbenchItem *workbenchItem = widget->item();
    QnWorkbenchItem *zoomedItem = workbench()->item(Qn::ZoomedRole);
    if(zoomedItem == workbenchItem) {
        if (action(Qn::ToggleTourModeAction)->isChecked()){
            action(Qn::ToggleTourModeAction)->toggle();
            return;
        }

        QRectF viewportGeometry = display()->viewportGeometry();
        QRectF zoomedItemGeometry = display()->itemGeometry(zoomedItem);

        if(viewportGeometry.width() < zoomedItemGeometry.width() * 0.975 || viewportGeometry.height() < zoomedItemGeometry.height() * 0.975) {
            workbench()->setItem(Qn::ZoomedRole, NULL);
            workbench()->setItem(Qn::ZoomedRole, workbenchItem);
        } else {
            workbench()->setItem(Qn::ZoomedRole, NULL);
        }
    } else {
        workbench()->setItem(Qn::ZoomedRole, workbenchItem);
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
    TRACE("SCENE LCLICKED");

    if(workbench() == NULL)
        return;

    workbench()->setItem(Qn::RaisedRole, NULL);
}

void QnWorkbenchController::at_scene_rightClicked(QGraphicsView *, const ClickInfo &info) {
    TRACE("SCENE RCLICKED");

    QScopedPointer<QMenu> menu(this->menu()->newMenu(Qn::SceneScope));
    if(menu->isEmpty())
        return;

    /* We don't want the curtain to kick in while menu is open. */
    menu->exec(info.screenPos());
}

void QnWorkbenchController::at_scene_doubleClicked(QGraphicsView *, const ClickInfo &) {
    TRACE("SCENE DOUBLECLICKED");

    if(workbench() == NULL)
        return;

    workbench()->setItem(Qn::ZoomedRole, NULL);
    display()->fitInView();
}

void QnWorkbenchController::at_display_widgetChanged(Qn::ItemRole role) {
    QnResourceWidget *newWidget = display()->widget(role);
    QnResourceWidget *oldWidget = m_widgetByRole[role];
    if(newWidget == oldWidget)
        return;

    m_widgetByRole[role] = newWidget;

    QGraphicsItem *focusItem = display()->scene()->focusItem();
    if(newWidget && (!focusItem || dynamic_cast<QnResourceWidget *>(focusItem)))
        newWidget->setFocus(); /* Move focus only if it's not already grabbed by some control element. */

    switch(role) {
    case Qn::ZoomedRole:
        m_zoomedToggle->setActive(newWidget != NULL);
        break;
    case Qn::RaisedRole:
    case Qn::CentralRole:
        if(newWidget) {
            m_cursorItem = newWidget->item();
            m_cursorPos = newWidget->item()->geometry().topLeft();
        }
        break;
    default:
        break;
    }
}

void QnWorkbenchController::at_display_widgetAdded(QnResourceWidget *widget) {
    widget->installEventFilter(this);

    connect(widget, SIGNAL(rotationStartRequested()),   this,   SLOT(at_widget_rotationStartRequested()));
    connect(widget, SIGNAL(rotationStopRequested()),    this,   SLOT(at_widget_rotationStopRequested()));
}

void QnWorkbenchController::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(widget == m_resizedWidget)
        m_resizedWidget = NULL;

    QnWorkbenchItem *item = widget->item();
    if(m_draggedWorkbenchItems.contains(item)) {
        m_draggedWorkbenchItems.removeOne(item);
        if(m_draggedWorkbenchItems.empty())
            m_moveInstrument->resetLater();
    }
    if(m_replacedWorkbenchItems.contains(item))
        m_replacedWorkbenchItems.removeOne(item);

    widget->removeEventFilter(this);

    disconnect(widget, NULL, this, NULL);
}

void QnWorkbenchController::at_widget_rotationStartRequested(QnResourceWidget *widget) {
    m_rotationInstrument->start(display()->view(), widget);
}

void QnWorkbenchController::at_widget_rotationStartRequested() {
    at_widget_rotationStartRequested(checked_cast<QnResourceWidget *>(sender()));
}

void QnWorkbenchController::at_widget_rotationStopRequested() {
    m_rotationInstrument->reset();
}

void QnWorkbenchController::at_selectAllAction_triggered() {
    foreach(QnResourceWidget *widget, display()->widgets())
        widget->setSelected(true);

    /* Move focus to scene if it's not there. */
    if(menu()->targetProvider() && menu()->targetProvider()->currentScope() != Qn::SceneScope)
        display()->scene()->setFocusItem(NULL);
}

void QnWorkbenchController::at_stopSmartSearchAction_triggered() {
    displayMotionGrid(menu()->currentParameters(sender()).widgets(), false);
}

void QnWorkbenchController::at_startSmartSearchAction_triggered() {
    displayMotionGrid(menu()->currentParameters(sender()).widgets(), true);
}

void QnWorkbenchController::at_checkFileSignatureAction_triggered() 
{
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if (widgets.isEmpty())
        return;
    QnResourceWidget *widget = widgets.at(0);
    if(widget->resource()->flags() & QnResource::network)
        return;
    QScopedPointer<SignDialog> dialog(new SignDialog(widget->resource()));
    dialog->setModal(true);
    dialog->exec();
}

void QnWorkbenchController::at_toggleSmartSearchAction_triggered() {
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();

    bool hidden = false;
    foreach(QnResourceWidget *widget, widgets)
        if((widget->resource()->flags() & QnResource::network) && !(widget->options() & QnResourceWidget::DisplayMotion)){
            hidden = true;
            break;
        }

    if(hidden) {
        at_startSmartSearchAction_triggered();
    } else {
        at_stopSmartSearchAction_triggered();
    }
}

void QnWorkbenchController::at_clearMotionSelectionAction_triggered() {
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();

    foreach(QnResourceWidget *widget, widgets)
        if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
            mediaWidget->clearMotionSelection();
}


void QnWorkbenchController::at_showInfoAction_triggered() {
    displayWidgetInfo(menu()->currentParameters(sender()).widgets(), true);
}

void QnWorkbenchController::at_hideInfoAction_triggered() {
    displayWidgetInfo(menu()->currentParameters(sender()).widgets(), false);
}

void QnWorkbenchController::at_toggleInfoAction_triggered() {
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();

    bool hidden = false;
    foreach(QnResourceWidget *widget, widgets)
        if(!(widget->options() & QnResourceWidget::DisplayInfo)){
            hidden = true;
            break;
        }

    if(hidden) {
        at_showInfoAction_triggered();
    } else {
        at_hideInfoAction_triggered();
    }
}


void QnWorkbenchController::at_maximizeItemAction_triggered() {
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();
    if(widgets.empty())
        return;

    workbench()->setItem(Qn::ZoomedRole, widgets[0]->item());
}

void QnWorkbenchController::at_unmaximizeItemAction_triggered() {
    workbench()->setItem(Qn::ZoomedRole, NULL);
}


void QnWorkbenchController::at_recordingAction_triggered(bool checked) {
    if(checked) {
        startRecording();
    } else {
        stopRecording();
    }
}

void QnWorkbenchController::at_fitInViewAction_triggered() {
    display()->fitInView();
}

void QnWorkbenchController::at_workbench_currentLayoutChanged() {
    // TODO: subscribe to permission changes.

    Qn::Permissions permissions = accessController()->permissions(workbench()->currentLayout()->resource());
    bool writable = permissions & Qn::WritePermission;

    m_moveInstrument->setEnabled(writable);
    m_resizingInstrument->setEnabled(writable);
}

void QnWorkbenchController::at_zoomedToggle_activated() {
    m_handScrollInstrument->setMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QnWorkbenchController::at_zoomedToggle_deactivated() {
    m_handScrollInstrument->setMouseButtons(Qt::RightButton);
}
