#ifndef QN_SCENE_CONTROLLER_PRIVATE_H
#define QN_SCENE_CONTROLLER_PRIVATE_H

#include "scenecontroller.h"
#include <QRect>

class QParallelAnimationGroup;

class AnimatedWidget;
class CentralWidget;
class CellLayout;
class BoundingInstrument;
class HandScrollInstrument;
class WheelZoomInstrument;
class DragInstrument;
class InstrumentManager;
class SetterAnimation;
class QnViewportAnimator;

class SceneControllerPrivate {
public:
    SceneControllerPrivate():
        q_ptr(NULL),
        scene(NULL),
        manager(NULL),
        centralWidget(NULL),
        centralLayout(NULL),
        focusedWidget(NULL),
        focusedExpanded(false),
        focusedZoomed(false)
    {}

    virtual ~SceneControllerPrivate() {}

private:
    void toggleFocus(AnimatedWidget *widget);
    void focusedExpand();
    void focusedContract();

    void toggleZoom(AnimatedWidget *widget);
    void focusedZoom();
    void focusedUnzoom();
    void updateBoundingRects();

    void toggleFocusedFitting();

    void fitInView();

    QRectF fitInViewRect() const;
    QRectF focusedWidgetRect() const;
    QRectF viewportRect() const;

private:
    SceneController *const q_ptr;

    /** Current mode. */
    SceneController::Mode mode;

    /** Graphics scene. */
    QGraphicsScene *scene;

    /** Graphics view. */
    QGraphicsView *view;

    /** Instrument manager for the scene. */
    InstrumentManager *manager;

    /** Central widget. */
    CentralWidget *centralWidget;

    /** Layout of the central widget. */
    CellLayout *centralLayout;

    /** Last bounds of central layout. */
    QRect lastCentralLayoutBounds;

    /** All animated widgets. */
    QList<AnimatedWidget *> animatedWidgets;

    /** Bounding instrument. */
    BoundingInstrument *boundingInstrument;

    /** Hand scroll instrument. */
    HandScrollInstrument *handScrollInstrument;

    /** Wheel zoom instrument. */
    WheelZoomInstrument *wheelZoomInstrument;

    /** Dragging instrument. */
    DragInstrument *dragInstrument;

    /** Widget that currently has focus. There can be only one such widget. */
    AnimatedWidget *focusedWidget;

    /** Whether the focused widget is expanded. */
    bool focusedExpanded;

    /** Whether the focused widget is zoomed. */
    bool focusedZoomed;

    /** Viewport animator. */
    QnViewportAnimator *viewportAnimator;

    Q_DECLARE_PUBLIC(SceneController);
};


#endif // QN_SCENE_CONTROLLER_PRIVATE_H
