#include "workbench_controller.h"

#include <cassert>
#include <cmath> /* For std::floor. */
#include <limits>

#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsView>
#include <QtOpenGL/QGLWidget>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QLabel>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtWidgets/QGraphicsProxyWidget>

#include <utils/common/util.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delete_later.h>
#include <utils/common/toggle.h>
#include <utils/common/log.h>
#include <utils/math/color_transformations.h>
#include <utils/resource_property_adaptors.h>

#include <core/resource/resource_directory_browser.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/file_processor.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>

#include <camera/resource_display.h>
#include <camera/cam_display.h>

#include "client/client_settings.h"

#include <ui/screen_recording/screen_recorder.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>

#include "ui/dialogs/sign_dialog.h" // TODO: move out.
#include <ui/dialogs/custom_file_dialog.h>  //for QnCustomFileDialog::fileDialogOptions() constant
#include <ui/dialogs/file_dialog.h>

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
#include <ui/graphics/instruments/zoom_window_instrument.h>

#include <ui/graphics/items/grid/grid_item.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/help/help_handler.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action_target_provider.h>

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

    /** Countdown value before screen recording starts. */
    const int recordingCountdownMs = 3000;
} // anonymous namespace


/*!
    Returns true if widget has checked option not set
*/
class ResourceWidgetHasNoOptionCondition
:
    public InstrumentItemCondition
{
public:
    ResourceWidgetHasNoOptionCondition( QnResourceWidget::Option optionToCheck )
    :
        m_optionToCheck( optionToCheck )
    {
    }

    //!Implementation of InstrumentItemCondition::oeprator()
    virtual bool operator()(QGraphicsItem *item, Instrument* /*instrument*/) const
    {
        QnResourceWidget* resourceWidget = dynamic_cast<QnResourceWidget*>(item);
        if( !resourceWidget )
            return true;
        return (resourceWidget->options() & m_optionToCheck) == 0;
    }

private:
    QnResourceWidget::Option m_optionToCheck;
};

QnWorkbenchController::QnWorkbenchController(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_manager(display()->instrumentManager()),
    m_selectionOverlayHackInstrumentDisabled(false),
    m_selectionOverlayHackInstrumentDisabled2(false),
    m_cursorPos(invalidCursorPos()),
    m_resizedWidget(NULL),
    m_dragDelta(invalidDragDelta()),
    m_screenRecorder(NULL),
    m_recordingCountdownLabel(NULL),
    m_tourModeHintLabel(NULL),
    m_menuEnabled(true)
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
    Instrument::EventTypeSet focusEventTypes = Instrument::makeSet(QEvent::FocusIn, QEvent::FocusOut);

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
    SignalingInstrument *sceneFocusSignalingInstrument = new SignalingInstrument(Instrument::Scene, Instrument::makeSet(QEvent::FocusIn), this);
    PtzInstrument *ptzInstrument = new PtzInstrument(this);
    ZoomWindowInstrument *zoomWindowInstrument = new ZoomWindowInstrument(this);

    gridAdjustmentInstrument->setSpeed(QSizeF(0.25 / 360.0, 0.25 / 360.0));
    gridAdjustmentInstrument->setMaxSpacing(QSizeF(0.5, 0.5));

    m_motionSelectionInstrument->setBrush(subColor(qnGlobals->mrsColor(), qnGlobals->selectionOpacityDelta()));
    m_motionSelectionInstrument->setPen(subColor(qnGlobals->mrsColor(), qnGlobals->selectionBorderDelta()));
    m_motionSelectionInstrument->setSelectionModifiers(Qt::ShiftModifier);

    m_rubberBandInstrument->setRubberBandZValue(display()->layerZValue(Qn::EffectsLayer));
    m_rotationInstrument->setRotationItemZValue(display()->layerZValue(Qn::EffectsLayer));
    m_resizingInstrument->setEffectRadius(8);

    m_rotationInstrument->addItemCondition(new InstrumentItemConditionAdaptor<IsInstanceOf<QnResourceWidget> >());
    m_rotationInstrument->addItemCondition(new ResourceWidgetHasNoOptionCondition( QnResourceWidget::WindowRotationForbidden ));

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

    m_manager->installInstrument(new StopInstrument(Instrument::Scene, keyEventTypes, this));
    m_manager->installInstrument(sceneKeySignalingInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::Scene, keyEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::Scene, keyEventTypes, this));

    m_manager->installInstrument(sceneFocusSignalingInstrument);

    /* View/viewport instruments. */
    m_manager->installInstrument(m_rotationInstrument);
    m_manager->installInstrument(m_handScrollInstrument);
    m_manager->installInstrument(m_resizingInstrument);
    m_manager->installInstrument(m_moveInstrument);
    m_manager->installInstrument(m_dragInstrument);
    m_manager->installInstrument(m_rubberBandInstrument);
    m_manager->installInstrument(m_motionSelectionInstrument);
    m_manager->installInstrument(ptzInstrument);
    m_manager->installInstrument(zoomWindowInstrument);

    display()->setLayer(m_dropInstrument->surface(), Qn::BackLayer);

    connect(m_itemLeftClickInstrument,  SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_leftClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(m_itemLeftClickInstrument,  SIGNAL(doubleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                 this,                           SLOT(at_item_doubleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(ptzInstrument,              SIGNAL(doubleClicked(QnMediaResourceWidget *)),                                             this,                           SLOT(at_item_doubleClicked(QnMediaResourceWidget *)));
    connect(m_itemRightClickInstrument, SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_rightClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(itemMiddleClickInstrument,  SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),                       this,                           SLOT(at_item_middleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(sceneClickInstrument,       SIGNAL(clicked(QGraphicsView *, const ClickInfo &)),                                        this,                           SLOT(at_scene_clicked(QGraphicsView *, const ClickInfo &)));
    connect(sceneClickInstrument,       SIGNAL(doubleClicked(QGraphicsView *, const ClickInfo &)),                                  this,                           SLOT(at_scene_doubleClicked(QGraphicsView *, const ClickInfo &)));
    connect(m_moveInstrument,           SIGNAL(moveStarted(QGraphicsView *, QList<QGraphicsItem *>)),                               this,                           SLOT(at_moveStarted(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_moveInstrument,           SIGNAL(move(QGraphicsView *, const QPointF &)),                                             this,                           SLOT(at_move(QGraphicsView *, const QPointF &)));
    connect(m_moveInstrument,           SIGNAL(moveFinished(QGraphicsView *, QList<QGraphicsItem *>)),                              this,                           SLOT(at_moveFinished(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_resizingInstrument,       SIGNAL(resizingStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),                this,                           SLOT(at_resizingStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)));
    connect(m_resizingInstrument,       SIGNAL(resizing(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),                       this,                           SLOT(at_resizing(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)));
    connect(m_resizingInstrument,       SIGNAL(resizingFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),               this,                           SLOT(at_resizingFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)));
    connect(m_rotationInstrument,       SIGNAL(rotationStarted(QGraphicsView *, QGraphicsWidget *)),                                this,                           SLOT(at_rotationStarted(QGraphicsView *, QGraphicsWidget *)));
    connect(m_rotationInstrument,       SIGNAL(rotationFinished(QGraphicsView *, QGraphicsWidget *)),                               this,                           SLOT(at_rotationFinished(QGraphicsView *, QGraphicsWidget *)));
    connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 this,                           SLOT(at_motionSelectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)));
    connect(m_motionSelectionInstrument, SIGNAL(selectionStarted(QGraphicsView *, QnMediaResourceWidget *)),                        this,                           SLOT(at_motionSelectionStarted(QGraphicsView *, QnMediaResourceWidget *)));
    connect(m_motionSelectionInstrument, SIGNAL(motionRegionSelected(QGraphicsView *, QnMediaResourceWidget *, const QRect &)),     this,                           SLOT(at_motionRegionSelected(QGraphicsView *, QnMediaResourceWidget *, const QRect &)));
    connect(m_motionSelectionInstrument, SIGNAL(motionRegionCleared(QGraphicsView *, QnMediaResourceWidget *)),                     this,                           SLOT(at_motionRegionCleared(QGraphicsView *, QnMediaResourceWidget *)));
    connect(sceneKeySignalingInstrument, SIGNAL(activated(QGraphicsScene *, QEvent *)),                                             this,                           SLOT(at_scene_keyPressed(QGraphicsScene *, QEvent *)));
    connect(sceneFocusSignalingInstrument, SIGNAL(activated(QGraphicsScene *, QEvent *)),                                           this,                           SLOT(at_scene_focusIn(QGraphicsScene *, QEvent *)));
    connect(zoomWindowInstrument,       SIGNAL(zoomRectChanged(QnMediaResourceWidget *, const QRectF &)),                           this,                           SLOT(at_zoomRectChanged(QnMediaResourceWidget *, const QRectF &)));
    connect(zoomWindowInstrument,       SIGNAL(zoomRectCreated(QnMediaResourceWidget *, const QColor &, const QRectF &)),           this,                           SLOT(at_zoomRectCreated(QnMediaResourceWidget *, const QColor &, const QRectF &)));
    connect(zoomWindowInstrument,       SIGNAL(zoomTargetChanged(QnMediaResourceWidget *, const QRectF &, QnMediaResourceWidget *)),this,                           SLOT(at_zoomTargetChanged(QnMediaResourceWidget *, const QRectF &, QnMediaResourceWidget *)));

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

    connect(m_rubberBandInstrument,     SIGNAL(rubberBandStarted(QGraphicsView *)),                                                 selectionOverlayHackInstrument, SLOT(recursiveDisable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandFinished(QGraphicsView *)),                                                selectionOverlayHackInstrument, SLOT(recursiveEnable()));

    connect(m_dragInstrument,           SIGNAL(dragProcessStarted(QGraphicsView *)),                                                m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_dragInstrument,           SIGNAL(dragProcessFinished(QGraphicsView *)),                                               m_moveInstrument,               SLOT(recursiveEnable()));

    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        m_rubberBandInstrument,         SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         ptzInstrument,                  SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        ptzInstrument,                  SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),         m_motionSelectionInstrument,    SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *)),        m_motionSelectionInstrument,    SLOT(recursiveEnable()));

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
    //connect(m_motionSelectionInstrument, SIGNAL(selectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *)),                 m_resizingInstrument,           SLOT(recursiveDisable()));
    //connect(m_motionSelectionInstrument, SIGNAL(selectionProcessFinished(QGraphicsView *, QnMediaResourceWidget *)),                m_resizingInstrument,           SLOT(recursiveEnable()));

    connect(ptzInstrument,              SIGNAL(ptzProcessStarted(QnMediaResourceWidget *)),                                         m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessFinished(QnMediaResourceWidget *)),                                        m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessStarted(QnMediaResourceWidget *)),                                         m_moveInstrument,               SLOT(recursiveDisable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessFinished(QnMediaResourceWidget *)),                                        m_moveInstrument,               SLOT(recursiveEnable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessStarted(QnMediaResourceWidget *)),                                         m_motionSelectionInstrument,    SLOT(recursiveDisable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessFinished(QnMediaResourceWidget *)),                                        m_motionSelectionInstrument,    SLOT(recursiveEnable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessStarted(QnMediaResourceWidget *)),                                         m_itemLeftClickInstrument,      SLOT(recursiveDisable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessFinished(QnMediaResourceWidget *)),                                        m_itemLeftClickInstrument,      SLOT(recursiveEnable()));
    connect(ptzInstrument,              SIGNAL(ptzProcessStarted(QnMediaResourceWidget *)),                                         this,                           SLOT(at_ptzProcessStarted(QnMediaResourceWidget *)));

    connect(zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_moveInstrument,               SLOT(recursiveDisable()));
    connect(zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_moveInstrument,               SLOT(recursiveEnable()));
    connect(zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_motionSelectionInstrument,    SLOT(recursiveDisable()));
    connect(zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_motionSelectionInstrument,    SLOT(recursiveEnable()));
    connect(zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  m_itemLeftClickInstrument,      SLOT(recursiveDisable()));
    connect(zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 m_itemLeftClickInstrument,      SLOT(recursiveEnable()));
    connect(zoomWindowInstrument,       SIGNAL(zoomWindowProcessStarted(QnMediaResourceWidget *)),                                  ptzInstrument,                  SLOT(recursiveDisable()));
    connect(zoomWindowInstrument,       SIGNAL(zoomWindowProcessFinished(QnMediaResourceWidget *)),                                 ptzInstrument,                  SLOT(recursiveEnable()));

    /* Connect to display. */
    connect(display(),                  SIGNAL(widgetChanged(Qn::ItemRole)),                                                        this,                           SLOT(at_display_widgetChanged(Qn::ItemRole)));
    connect(display(),                  SIGNAL(widgetAdded(QnResourceWidget *)),                                                    this,                           SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display(),                  SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)),                                         this,                           SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(workbench(),                SIGNAL(currentLayoutAboutToBeChanged()),                                                    this,                           SLOT(at_workbench_currentLayoutAboutToBeChanged()));
    connect(workbench(),                SIGNAL(currentLayoutChanged()),                                                             this,                           SLOT(at_workbench_currentLayoutChanged()));

    /* Set up zoom toggle. */
    //m_wheelZoomInstrument->recursiveDisable();
    m_zoomedToggle = new QnToggle(false, this);
    connect(m_zoomedToggle,             SIGNAL(activated()),                                                                        m_moveInstrument,               SLOT(recursiveDisable()));
    connect(m_zoomedToggle,             SIGNAL(deactivated()),                                                                      m_moveInstrument,               SLOT(recursiveEnable()));
    connect(m_zoomedToggle,             SIGNAL(activated()),                                                                        m_resizingInstrument,           SLOT(recursiveDisable()));
    connect(m_zoomedToggle,             SIGNAL(deactivated()),                                                                      m_resizingInstrument,           SLOT(recursiveEnable()));
    connect(m_zoomedToggle,             SIGNAL(activated()),                                                                        m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_zoomedToggle,             SIGNAL(deactivated()),                                                                      m_rubberBandInstrument,         SLOT(recursiveEnable()));
    //connect(m_zoomedToggle,             SIGNAL(activated()),                                                                        m_wheelZoomInstrument,          SLOT(recursiveEnable()));
    //connect(m_zoomedToggle,             SIGNAL(deactivated()),                                                                      m_wheelZoomInstrument,          SLOT(recursiveDisable()));
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
    connect(action(Qn::ToggleTourModeAction), SIGNAL(triggered(bool)),                                                              this,                           SLOT(at_toggleTourModeAction_triggered(bool)));

    /* Init screen recorder. */
    if (QnScreenRecorder::isSupported()){
        m_screenRecorder = new QnScreenRecorder(this);
        connect(m_screenRecorder,       SIGNAL(recordingStarted()),                                                                 this,                           SLOT(at_screenRecorder_recordingStarted()));
        connect(m_screenRecorder,       SIGNAL(recordingFinished(QString)),                                                         this,                           SLOT(at_screenRecorder_recordingFinished(QString)));
        connect(m_screenRecorder,       SIGNAL(error(QString)),                                                                     this,                           SLOT(at_screenRecorder_error(QString)));
    }

    connect(accessController(), SIGNAL(permissionsChanged(const QnResourcePtr &)),                                                  this,                           SLOT(at_accessController_permissionsChanged(const QnResourcePtr &)));
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
        if(!widget->resource()->hasFlags(Qn::motion))
            continue;
        if (!widget->zoomRect().isNull())
            continue;
        widget->setOption(QnResourceWidget::DisplayMotion, display);
    }
}

void QnWorkbenchController::displayWidgetInfo(const QList<QnResourceWidget *> &widgets, bool display){
    foreach(QnResourceWidget *widget, widgets)
        widget->setInfoVisible(display);
}

void QnWorkbenchController::moveCursor(const QPoint &aAxis, const QPoint &bAxis) {
    QnWorkbenchItem *centerItem = m_cursorItem.data();
    if(!centerItem)
        centerItem = workbench()->currentLayout()->item(m_cursorPos);

    QPoint center = m_cursorPos;
    if(centerItem && !centerItem->geometry().contains(center))
        center = centerItem->geometry().topLeft();

    QRect boundingRect = workbench()->currentLayout()->boundingRect();
    if(boundingRect.isEmpty())
        return;

    QPoint aReturn = -aAxis * qAbs(dot(toPoint(boundingRect.size()), aAxis) / dot(aAxis, aAxis));
    QPoint bReturn = -bAxis * qAbs(dot(toPoint(boundingRect.size()), bAxis) / dot(bAxis, bAxis));

    QPoint pos = center;
    QnWorkbenchItem *item = NULL;
    while(true) {
        pos += aAxis;
        if(!boundingRect.contains(pos))
            pos += aReturn + bAxis;
        if(!boundingRect.contains(pos))
            pos += bReturn;
        if(pos == center)
            return; /* No other items on layout. */

        item = workbench()->currentLayout()->item(pos);
        if(item && item != centerItem)
            break;
    }

    Qn::ItemRole role = Qn::ZoomedRole;
    if(!workbench()->item(role))
        role = Qn::RaisedRole;

    display()->scene()->clearSelection();
    display()->widget(item)->setSelected(true);
    workbench()->setItem(role, item);
    m_cursorPos = pos;
    m_cursorItem = item;
}

void QnWorkbenchController::showContextMenuAt(const QPoint &pos){
    if(!m_menuEnabled)
        return;

    QMetaObject::invokeMethod(this, "showContextMenuAtInternal", Qt::QueuedConnection,
                              Q_ARG(QPoint, pos), Q_ARG(WeakGraphicsItemPointerList, display()->scene()->selectedItems()));
}

void QnWorkbenchController::showContextMenuAtInternal(const QPoint &pos, const WeakGraphicsItemPointerList &selectedItems) {
    QScopedPointer<QMenu> menu(this->menu()->newMenu(Qn::SceneScope, mainWindow(), selectedItems.materialized()));
    if(menu->isEmpty())
        return;

    menu->exec(pos);
}

// -------------------------------------------------------------------------- //
// Screen recording
// -------------------------------------------------------------------------- //
void QnWorkbenchController::startRecording() {
    if (!m_screenRecorder) {
        action(Qn::ToggleScreenRecordingAction)->setChecked(false);
        return;
    }

    if(m_screenRecorder->isRecording() || (m_recordingCountdownLabel != NULL)) {
        action(Qn::ToggleScreenRecordingAction)->setChecked(false);
        return;
    }

    action(Qn::ToggleScreenRecordingAction)->setChecked(true);

    m_recordingCountdownLabel = QnGraphicsMessageBox::informationTicking(tr("Recording in...%1"), recordingCountdownMs);
    connect(m_recordingCountdownLabel, &QnGraphicsMessageBox::finished, this, &QnWorkbenchController::at_recordingAnimation_finished);
}

void QnWorkbenchController::stopRecording() {
    if (m_recordingCountdownLabel) {
        disconnect(m_recordingCountdownLabel, NULL, this, NULL);
        m_recordingCountdownLabel->hideImmideately();
        m_recordingCountdownLabel = NULL;
    }

    action(Qn::ToggleScreenRecordingAction)->setChecked(false);

    if (m_screenRecorder)
        m_screenRecorder->stopRecording();
}

void QnWorkbenchController::at_recordingAnimation_finished() {
    if (m_recordingCountdownLabel)
        m_recordingCountdownLabel->setOpacity(0.0);
    m_recordingCountdownLabel = NULL;
    if (m_screenRecorder) // just in case =)
        m_screenRecorder->startRecording();
}


void QnWorkbenchController::at_screenRecorder_recordingStarted() {
    if (!m_recordingCountdownLabel)
        return;
    m_recordingCountdownLabel->setOpacity(0.0);
}

void QnWorkbenchController::at_screenRecorder_error(const QString &errorMessage) {
    if (QnScreenRecorder::isSupported())
        action(Qn::ToggleScreenRecordingAction)->setChecked(false);

    QMessageBox::warning(display()->view(), tr("Warning"), tr("Can't start recording due to the following error: %1").arg(errorMessage));
}

void QnWorkbenchController::at_screenRecorder_recordingFinished(const QString &recordedFileName) {
    QString suggetion = QFileInfo(recordedFileName).fileName();
    if (suggetion.isEmpty())
        suggetion = tr("Recorded Video");

    while (true) {
        QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
                                                      display()->view(),
                                                      tr("Save Recording As..."),
                                                      qnSettings->lastRecordingDir() + QLatin1Char('/') + suggetion,
                                                      tr("AVI (Audio/Video Interleaved) (*.avi)")));
        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setAcceptMode(QFileDialog::AcceptSave);
        int dialogResult = dialog->exec();

        QString filePath = dialog->selectedFile();
        if (dialogResult == QDialog::Accepted && !filePath.isEmpty()) {
            QString selectedExtension = dialog->selectedExtension();
            if (!filePath.endsWith(selectedExtension, Qt::CaseInsensitive))
                filePath += selectedExtension;

            QFile::remove(filePath);
            if (!QFile::rename(recordedFileName, filePath)) {
                QString message = tr("Could not overwrite file '%1'. Please try another name.").arg(filePath);
                CL_LOG(cl_logWARNING) cl_log.log(message, cl_logWARNING);
                QMessageBox::warning(display()->view(), tr("Warning"), message, QMessageBox::Ok, QMessageBox::NoButton);
                continue;
            }

            QnFileProcessor::createResourcesForFile(filePath);

            qnSettings->setLastRecordingDir(QFileInfo(filePath).absolutePath());
        } else {
            QFile::remove(recordedFileName);
        }
        break;
    }
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
        QnResourceWidget *widget = display()->widget(Qn::CentralRole);
        if(widget && widget == display()->widget(Qn::ZoomedRole)) {
            menu()->trigger(Qn::UnmaximizeItemAction, widget);
        } else {
            menu()->trigger(Qn::MaximizeItemAction, widget);
        }
        break;
    }
    case Qt::Key_Up:
        if (e->modifiers() & Qt::AltModifier)
            m_handScrollInstrument->emulate(QPoint(0, -15));
        else
            moveCursor(QPoint(0, -1), QPoint(-1, 0));
        break;
    case Qt::Key_Down:
        if (e->modifiers() & Qt::AltModifier)
            m_handScrollInstrument->emulate(QPoint(0, 15));
        else
            moveCursor(QPoint(0, 1), QPoint(1, 0));
        break;
    case Qt::Key_Left:
        if (e->modifiers() & Qt::AltModifier)
            m_handScrollInstrument->emulate(QPoint(-15, 0));
        else
            moveCursor(QPoint(-1, 0), QPoint(0, -1));
        break;
    case Qt::Key_Right:
        if (e->modifiers() & Qt::AltModifier)
            m_handScrollInstrument->emulate(QPoint(15, 0));
        else
            moveCursor(QPoint(1, 0), QPoint(0, 1));
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        m_wheelZoomInstrument->emulate(30);
        break;
    case Qt::Key_Minus:
        m_wheelZoomInstrument->emulate(-30);
        break;
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
        break; /* Don't let the view handle these and scroll. */
    case Qt::Key_Menu: {
        QGraphicsView *view = display()->view();        
        QList<QGraphicsItem *> items = display()->scene()->selectedItems();
        QPoint offset = view->mapToGlobal(QPoint(0, 0));
        if (items.count() == 0) {
            showContextMenuAt(offset);
        } else {
            QRectF rect = items[0]->mapToScene(items[0]->boundingRect()).boundingRect();
            QRect testRect = QnSceneTransformations::mapRectFromScene(view, rect); /* Where is the static analogue? */ 
            showContextMenuAt(offset + testRect.bottomRight());
        }
        break;
    }
    case Qt::Key_0:
    case Qt::Key_1:
    case Qt::Key_2:
    case Qt::Key_3:
    case Qt::Key_4:
    case Qt::Key_5:
    case Qt::Key_6:
    case Qt::Key_7:
    case Qt::Key_8:
    case Qt::Key_9: {
        QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget*>(display()->widget(Qn::CentralRole));
        if(!widget || !widget->ptzController())
            break;

        int hotkey = e->key() - Qt::Key_0;

        QnPtzHotkeysResourcePropertyAdaptor adaptor;
        adaptor.setResource(widget->resource()->toResourcePtr());

        QString objectId = adaptor.value().value(hotkey);
        if (objectId.isEmpty())
            break;

        menu()->trigger(Qn::PtzActivateObjectAction, QnActionParameters(widget).withArgument(Qn::PtzObjectIdRole, objectId));
        break;
    }
    default:
        event->ignore(); /* Wasn't recognized? Ignore. */
        break;
    }
}

void QnWorkbenchController::at_scene_focusIn(QGraphicsScene *, QEvent *event) {
    // TODO: #Elric evil hack to prevent focus jumps when scene is focused.
    QFocusEvent *focusEvent = static_cast<QFocusEvent *>(event);
    *focusEvent = QFocusEvent(focusEvent->type(), Qt::OtherFocusReason);
}

void QnWorkbenchController::at_resizingStarted(QGraphicsView *, QGraphicsWidget *item, ResizingInfo *) {
    TRACE("RESIZING STARTED");

    m_resizedWidget = qobject_cast<QnResourceWidget *>(item);
    if(m_resizedWidget == NULL)
        return;

    m_selectionOverlayHackInstrumentDisabled = true;
    display()->selectionOverlayHackInstrument()->recursiveDisable();

    workbench()->setItem(Qn::RaisedRole, NULL); /* Un-raise currently raised item so that it doesn't interfere with resizing. */

    display()->bringToFront(m_resizedWidget);
    opacityAnimator(display()->gridItem())->animateTo(1.0);
    opacityAnimator(m_resizedWidget)->animateTo(widgetManipulationOpacity);
}

void QnWorkbenchController::at_resizing(QGraphicsView *, QGraphicsWidget *item, ResizingInfo *) {
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

        display()->gridItem()->setCellState(m_resizedWidgetRect, QnGridItem::Initial);
        display()->gridItem()->setCellState(disposition.free, QnGridItem::Allowed);
        display()->gridItem()->setCellState(disposition.occupied, QnGridItem::Disallowed);

        m_resizedWidgetRect = newResizingWidgetRect;
    }
}

void QnWorkbenchController::at_resizingFinished(QGraphicsView *, QGraphicsWidget *item, ResizingInfo *) {
    TRACE("RESIZING FINISHED");

    opacityAnimator(display()->gridItem())->animateTo(0.0);
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
    display()->gridItem()->setCellState(m_resizedWidgetRect, QnGridItem::Initial);
    m_resizedWidgetRect = QRect();
    m_resizedWidget = NULL;
    if(m_selectionOverlayHackInstrumentDisabled) {
        display()->selectionOverlayHackInstrument()->recursiveEnable();
        m_selectionOverlayHackInstrumentDisabled = false;
    }
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

    m_selectionOverlayHackInstrumentDisabled2 = true;
    display()->selectionOverlayHackInstrument()->recursiveDisable();

    /* Bring to front preserving relative order. */
    display()->bringToFront(items);
    display()->setLayer(items, Qn::FrontLayer);

    /* Show grid. */
    opacityAnimator(display()->gridItem())->animateTo(1.0);
}

void QnWorkbenchController::at_move(QGraphicsView *, const QPointF &totalDelta) {
    if(m_draggedWorkbenchItems.empty())
        return;

    QnWorkbenchLayout *layout = m_draggedWorkbenchItems[0]->layout();

    QPoint newDragDelta = mapper()->mapDeltaToGridF(totalDelta).toPoint();
    if(newDragDelta != m_dragDelta) {
        display()->gridItem()->setCellState(m_dragGeometries, QnGridItem::Initial);

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

        display()->gridItem()->setCellState(disposition.free, QnGridItem::Allowed);
        display()->gridItem()->setCellState(disposition.occupied, QnGridItem::Disallowed);
    }
}

void QnWorkbenchController::at_moveFinished(QGraphicsView *, const QList<QGraphicsItem *> &) {
    TRACE("MOVE FINISHED");

    /* Hide grid. */
    opacityAnimator(display()->gridItem())->animateTo(0.0);

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
    display()->gridItem()->setCellState(m_dragGeometries, QnGridItem::Initial);
    m_dragDelta = invalidDragDelta();
    m_draggedWorkbenchItems.clear();
    m_replacedWorkbenchItems.clear();
    m_dragGeometries.clear();

    if(m_selectionOverlayHackInstrumentDisabled2) {
        display()->selectionOverlayHackInstrument()->recursiveEnable();
        m_selectionOverlayHackInstrumentDisabled2 = false;
    }
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

    resourceWidget->item()->setRotation(widget->rotation() - (resourceWidget->item()->data<bool>(Qn::ItemFlipRole, false) ? 180.0: 0.0));
}

void QnWorkbenchController::at_zoomRectChanged(QnMediaResourceWidget *widget, const QRectF &zoomRect) {
    widget->item()->setZoomRect(zoomRect);
}

void QnWorkbenchController::at_zoomRectCreated(QnMediaResourceWidget *widget, const QColor &color, const QRectF &zoomRect) {
    menu()->trigger(Qn::CreateZoomWindowAction, QnActionParameters(widget).withArgument(Qn::ItemZoomRectRole, zoomRect).withArgument(Qn::ItemFrameDistinctionColorRole, color));
    widget->setCheckedButtons(widget->checkedButtons() & ~QnMediaResourceWidget::ZoomWindowButton);
}

void QnWorkbenchController::at_zoomTargetChanged(QnMediaResourceWidget *widget, const QRectF &zoomRect, QnMediaResourceWidget *zoomTargetWidget) {
    QnLayoutItemData data = widget->item()->data();
    delete widget;

    data.uuid = QUuid::createUuid();
    data.resource.id = zoomTargetWidget->resource()->toResource()->getId();
    data.resource.path = zoomTargetWidget->resource()->toResource()->getUniqueId();
    data.zoomTargetUuid = zoomTargetWidget->item()->uuid();
    data.rotation = zoomTargetWidget->item()->rotation();
    data.zoomRect = zoomRect;
    data.dewarpingParams = zoomTargetWidget->item()->dewarpingParams();
    data.dewarpingParams.panoFactor = 1; // zoom target must always be dewarped by 90 degrees
    data.dewarpingParams.enabled = zoomTargetWidget->resource()->getDewarpingParams().enabled;  // zoom items on fisheye cameras must always be dewarped

    int maxItems = (qnSettings->lightMode() & Qn::LightModeSingleItem)
            ? 1
            : qnSettings->maxSceneVideoItems();

    QnLayoutResourcePtr layout = workbench()->currentLayout()->resource();
    if (layout->getItems().size() >= maxItems)
        return;
    layout->addItem(data);
}

void QnWorkbenchController::at_motionSelectionProcessStarted(QGraphicsView *, QnMediaResourceWidget *widget) {
    widget->setOption(QnResourceWidget::DisplayMotion, true);
}

void QnWorkbenchController::at_motionSelectionStarted(QGraphicsView *, QnMediaResourceWidget *widget) {
    foreach(QnResourceWidget *otherWidget, display()->widgets())
        if(otherWidget != widget)
            if(QnMediaResourceWidget *otherMediaWidget = dynamic_cast<QnMediaResourceWidget *>(otherWidget))
                otherMediaWidget->clearMotionSelection();
}

void QnWorkbenchController::at_motionRegionCleared(QGraphicsView *, QnMediaResourceWidget *widget) {
    widget->clearMotionSelection();
}

void QnWorkbenchController::at_motionRegionSelected(QGraphicsView *, QnMediaResourceWidget *widget, const QRect &region) {
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

    at_item_doubleClicked(widget);
}

void QnWorkbenchController::at_item_doubleClicked(QnMediaResourceWidget *widget) {
    at_item_doubleClicked(static_cast<QnResourceWidget *>(widget));
}

void QnWorkbenchController::at_item_doubleClicked(QnResourceWidget *widget) {
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

void QnWorkbenchController::at_scene_rightClicked(QGraphicsView *view, const ClickInfo &info) {
    TRACE("SCENE RCLICKED");

    view->scene()->clearSelection(); /* Just to feel safe. */

    showContextMenuAt(info.screenPos());
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
    if(widget->resource()->flags() & Qn::network)
        return;
    QScopedPointer<SignDialog> dialog(new SignDialog(widget->resource(), mainWindow()));
    dialog->setModal(true);
    dialog->exec();
}

void QnWorkbenchController::at_toggleSmartSearchAction_triggered() {
    QnResourceWidgetList widgets = menu()->currentParameters(sender()).widgets();

    foreach(QnResourceWidget *widget, widgets) {
        if (!widget->resource()->hasFlags(Qn::motion))
            continue;

        if (!widget->zoomRect().isNull())
            continue;

        if(!(widget->options() & QnResourceWidget::DisplayMotion)) {
            at_startSmartSearchAction_triggered();
            return;
        }
    }
    at_stopSmartSearchAction_triggered();
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

void QnWorkbenchController::at_toggleTourModeAction_triggered(bool checked) {
    if (!checked) {
        if (m_tourModeHintLabel) {
            m_tourModeHintLabel->hideImmideately();
            disconnect(m_tourModeHintLabel, NULL, this, NULL);
            m_tourModeHintLabel = NULL;
        }
        return;
    }
    m_tourModeHintLabel = QnGraphicsMessageBox::information(tr("Press any key to stop the tour"));
    connect(m_tourModeHintLabel, SIGNAL(finished()), this, SLOT(at_tourModeLabel_finished()));
}

void QnWorkbenchController::at_tourModeLabel_finished() {
    if (m_tourModeHintLabel)
       m_tourModeHintLabel = NULL;
}

void QnWorkbenchController::at_fitInViewAction_triggered() {
    display()->fitInView();
}

void QnWorkbenchController::at_workbench_currentLayoutAboutToBeChanged() {
    QnWorkbenchLayout *layout = workbench()->currentLayout();
    if (!layout || !layout->resource())
        return;

    disconnect(layout->resource(), NULL, this, NULL);
}

void QnWorkbenchController::at_workbench_currentLayoutChanged() {
    QnWorkbenchLayout *layout = workbench()->currentLayout();
    if (!layout)
        return;
    if (layout->resource()) {
        connect(layout->resource(), SIGNAL(lockedChanged(const QnLayoutResourcePtr &)), this, SLOT(updateLayoutInstruments(const QnLayoutResourcePtr &)));
    }
    updateLayoutInstruments(layout->resource());
}

void QnWorkbenchController::at_accessController_permissionsChanged(const QnResourcePtr &resource) {
    QnWorkbenchLayout *layout = workbench()->currentLayout();
    if (!layout || !layout->resource() || layout->resource() != resource)
        return;
    updateLayoutInstruments(resource.dynamicCast<QnLayoutResource>());
}

void QnWorkbenchController::at_zoomedToggle_activated() {
    m_handScrollInstrument->setMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QnWorkbenchController::at_zoomedToggle_deactivated() {
    m_handScrollInstrument->setMouseButtons(Qt::RightButton);
}

void QnWorkbenchController::updateLayoutInstruments(const QnLayoutResourcePtr &layout) {
    if (!layout) {
        m_moveInstrument->setEnabled(false);
        m_resizingInstrument->setEnabled(false);
        return;
    }

    Qn::Permissions permissions = accessController()->permissions(layout);
    bool writable = permissions & Qn::WritePermission;

    m_moveInstrument->setEnabled(writable && !layout->locked());
    m_resizingInstrument->setEnabled(writable && !layout->locked());
}

void QnWorkbenchController::at_ptzProcessStarted(QnMediaResourceWidget *widget) {
    if(widget == NULL)
        return;

    // skip method if current widget is already raised or zoomed
    if (display()->widget(Qn::RaisedRole) == widget
        || display()->widget(Qn::ZoomedRole) == widget)
        return;

    workbench()->setItem(Qn::RaisedRole, NULL); /* Un-raise currently raised item so that it doesn't interfere with ptz. */
    display()->scene()->clearSelection();
    widget->setSelected(true);
    display()->bringToFront(widget);
}
