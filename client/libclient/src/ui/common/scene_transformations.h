#ifndef QN_SCENE_TRANSFORMATIONS_H
#define QN_SCENE_TRANSFORMATIONS_H

#include <QtWidgets/QGraphicsView>

class QnSceneTransformations {
public:
    /**
     * Moves the given viewport.
     * 
     * \param view                      Graphics view to move viewport of.
     * \param viewportPositionDelta     Move delta, in viewport coordinates.
     */
    static void moveViewport(QGraphicsView *view, const QPoint &viewportPositionDelta);

    /**
     * Moves the given viewport.
     * 
     * \param view                      Graphics view to move viewport of.
     * \param viewportPositionDelta     Move delta, in viewport coordinates.
     */
    static void moveViewport(QGraphicsView *view, const QPointF &viewportPositionDelta);

    /**
     * Moves the given viewport.
     * 
     * \param view                      Graphics view to move viewport of.
     * \param scenePositionDelta        Move delta, in scene coordinates.
     */
    static void moveViewportScene(QGraphicsView *view, const QPointF &scenePositionDelta);

    /**
     * Centers the given viewport on the given position.
     * 
     * \param view                      Graphics view to center viewport of.
     * \param sceneCenter               Position to center on, in scene coordinates.
     */
    static void moveViewportSceneTo(QGraphicsView *view, const QPointF &sceneCenter);

    /**
     * Scales the given viewport.
     * 
     * \param view                      Graphics view to scale viewport of.
     * \param factor                    Viewport scale factor.
     * \param anchor                    Transformation anchor.
     */
    static void scaleViewport(QGraphicsView *view, qreal factor, QGraphicsView::ViewportAnchor anchor = QGraphicsView::AnchorViewCenter);

    /**
     * Scales the given viewport.
     * 
     * \param view                      Graphics view to scale viewport of.
     * \param factor                    Viewport scale factor.
     * \param sceneAnchor               Transformation anchor, in viewport coordinates.
     */
    static void scaleViewport(QGraphicsView *view, qreal factor, const QPoint &viewportAnchor);

    /**
     * Scales the given viewport to the given size.
     * 
     * \param view                      Graphics view to scale viewport of.
     * \param size                      Size to scale to.
     * \param mode                      Aspect ratio mode.
     * \param anchor                    Transformation anchor.
     */
    static void scaleViewportTo(QGraphicsView *view, const QSizeF &size, Qt::AspectRatioMode mode, QGraphicsView::ViewportAnchor anchor = QGraphicsView::AnchorViewCenter);

    /**
     * \param view                      Graphics view. Must not be NULL.
     * \param rect                      Rectangle to map to scene.
     * \returns                         Bounding rectangle of the mapped polygon.
     */
    static QRectF mapRectToScene(const QGraphicsView *view, const QRect &rect);

    /**
     * \param view                      Graphics view. Must not be NULL.
     * \param rect                      Rectangle to map to scene.
     * \returns                         Bounding rectangle of the mapped polygon.
     */
    static QRectF mapRectToScene(const QGraphicsView *view, const QRectF &rect);

    /**
     * \param view                      Graphics view. Must not be NULL.
     * \param rect                      Rectangle to map from scene.
     * \returns                         Bounding rectangle of the mapped polygon.
     */
    static QRect mapRectFromScene(const QGraphicsView *view, const QRectF &rect);


}; // class QnSceneTransformations

#endif // QN_SCENE_TRANSFORMATIONS_H
