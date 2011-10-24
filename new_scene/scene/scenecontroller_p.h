#ifndef QN_SCENE_CONTROLLER_PRIVATE_H
#define QN_SCENE_CONTROLLER_PRIVATE_H

#include "scenecontroller.h"
#include <QRect>

class AnimatedWidget;
class CentralWidget;
class CellLayout;
class BoundingInstrument;
class InstrumentManager;
class ViewportAnimation;

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
    void toggleFocus(QGraphicsView *view, AnimatedWidget *widget);
    void toggleZoom(QGraphicsView *view, AnimatedWidget *widget);

    void addView(QGraphicsView *view);

    void focusedExpand(QGraphicsView *view);
    void focusedContract(QGraphicsView *view);

    void focusedZoom(QGraphicsView *view);
    void focusedUnzoom(QGraphicsView *view);

private:
    SceneController *const q_ptr;

    /** Current mode. */
    SceneController::Mode mode;

    /** Graphics scene. */
    QGraphicsScene *scene;

    /** Viewport animation. */
    ViewportAnimation *viewportAnimation;

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

    /** Rectangle to unzoom to. */
    QRectF unzoomRect;

    Q_DECLARE_PUBLIC(SceneController);
};


#endif // QN_SCENE_CONTROLLER_PRIVATE_H
