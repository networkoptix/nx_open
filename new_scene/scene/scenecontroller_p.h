#ifndef QN_SCENE_CONTROLLER_PRIVATE_H
#define QN_SCENE_CONTROLLER_PRIVATE_H

#include "scenecontroller.h"
#include <QRect>

class QParallelAnimationGroup;

class AnimatedWidget;
class CentralWidget;
class CellLayout;
class BoundingInstrument;
class InstrumentManager;
class SetterAnimation;

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
        focusedZoomed(false),
        zoomAnimation(NULL),
        scaleAnimation(NULL),
        positionAnimation(NULL)
    {}

    virtual ~SceneControllerPrivate() {}

private:
    void toggleFocus(QGraphicsView *view, AnimatedWidget *widget);
    void toggleZoom(QGraphicsView *view, AnimatedWidget *widget);

    void addView(QGraphicsView *view);

    void focusedExpand(QGraphicsView *view);
    void focusedContract(QGraphicsView *view);

    void toggleFocusedFitting(QGraphicsView *view);
    void focusedZoom(QGraphicsView *view);
    void focusedUnzoom(QGraphicsView *view);
    void initZoomAnimations(const QRectF &unzoomed, const QRectF &zoomed);
    void finishZoom();

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

    /** Widget that currently has focus. There can be only one such widget. */
    AnimatedWidget *focusedWidget;

    /** Whether the focused widget is expanded. */
    bool focusedExpanded;

    /** Whether the focused widget is zoomed. */
    bool focusedZoomed;

    /** Viewport animation. */
    QParallelAnimationGroup *zoomAnimation;

    /** Viewport scale animation. */
    SetterAnimation *scaleAnimation;

    /** Viewport position animation. */
    SetterAnimation *positionAnimation;

    Q_DECLARE_PUBLIC(SceneController);
};


#endif // QN_SCENE_CONTROLLER_PRIVATE_H
