#ifndef QN_SCENE_UTILITY_H
#define QN_SCENE_UTILITY_H

#include <Qt>
#include <QGraphicsView>
#include <QPoint>
#include <QSize>

class QRect;
class QRectF;
class QSizeF;
class QTransform;

class QnSceneUtility {
public:
    /**
     * \param                           Size.
     * \returns                         Given size converted to a point.
     */
    static QPointF toPoint(const QSizeF &size) {
        return QPointF(size.width(), size.height());
    }

    /**
     * \param                           Size.
     * \returns                         Given size converted to a point.
     */
    static QPoint toPoint(const QSize &size) {
        return QPoint(size.width(), size.height());
    }

    static qreal length(const QPointF &point);

    static qreal length(const QSizeF &size);

    /**
     * \param view                      Graphics view. Must not be NULL.
     * \param rect                      Rectangle to map to scene.
     * \returns                         Bounding rectangle of the mapped polygon.
     */
    static QRectF mapRectToScene(QGraphicsView *view, const QRect &rect);

    /**
     * \param view                      Graphics view. Must not be NULL.
     * \param rect                      Rectangle to map from scene.
     * \returns                         Bounding rectangle of the mapped polygon.
     */
    static QRect mapRectFromScene(QGraphicsView *view, const QRectF &rect);

    /**
     * Calculates the factor by which the given size must be scaled to fit into
     * the given bounds.
     * 
     * Note that <tt>Qt::IgnoreAspectRatio</tt> mode is treated the same way
     * as <tt>Qt::KeepAspectRatio</tt> mode.
     * 
     * \param size                      Size to scale.
     * \param bounds                    Size bounds.
     * \param mode                      Aspect ratio mode.
     * \returns                         Scale factor.
     */
    static qreal scaleFactor(QSizeF size, QSizeF bounds, Qt::AspectRatioMode mode);

    /**
     * Bounds the given size, so that a rectangle of size maxSize would include
     * a rectangle of bounded size.
     *
     * \param size                      Size to bound.
     * \param maxSize                   Maximal size. 
     * \param mode                      Aspect ratio mode.
     */
    static QSizeF bounded(const QSizeF &size, const QSizeF &maxSize, Qt::AspectRatioMode mode);

    /**
     * Expands the given size to given minSize.
     * 
     * \param size                      Size to expand.
     * \param maxSize                   Minimal size. 
     * \param mode                      Aspect ratio mode.
     */
    static QSizeF expanded(const QSizeF &size, const QSizeF &minSize, Qt::AspectRatioMode mode);

    /**
     * Expands an infinitely small size with the given aspect ratio (width to
     * height ratio) to given minSize.
     * 
     * \param aspectRatio               Aspect ratio.
     * \param minSize                   Minimal size.
     * \param mode                      Aspect ratio mode.
     */
    static QSizeF expanded(qreal aspectRatio, const QSizeF &minSize, Qt::AspectRatioMode mode);

    /**
     * Dilates the given rectangle by the given amount.
     * 
     * \param rect                      Rectangle to dilate.
     * \param amount                    Dilation amount.
     * \returns                         Dilated rectangle.
     */
    static QRectF dilated(const QRectF &rect, const QSizeF &amount);

    /**
     * \param size                      Size to check for containment.
     * \param otherSize                 Reference size.
     * \returns                         Whether the reference size contains the given size.
     */
    static bool contains(const QSizeF &size, const QSizeF &otherSize);

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
     * \param scenePositionDelta        Move delta, in scene coordinates.
     */
    static void moveViewport(QGraphicsView *view, const QPointF &scenePositionDelta);

    /**
     * Centers the given viewport on the given position.
     * 
     * \param view                      Graphics view to center viewport of.
     * \param centerPosition            Position to center on.
     */
    static void moveViewportTo(QGraphicsView *view, const QPointF &centerPosition);

    /**
     * Scales the given viewport.
     * 
     * \param view                      Graphics view to scale viewport of.
     * \param factor                    Viewport scale factor.
     * \param anchor                    Transformation anchor.
     */
    static void scaleViewport(QGraphicsView *view, qreal factor, QGraphicsView::ViewportAnchor anchor = QGraphicsView::AnchorViewCenter);

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
     * \param viewport                 Viewport. Must not be NULL and must actually be a viewport.
     * \returns                        Graphics view of the given viewport.
     */
    static QGraphicsView *view(QWidget *viewport);

};

inline bool qFuzzyIsNull(const QPointF &p) {
    return ::qFuzzyIsNull(p.x()) && ::qFuzzyIsNull(p.y());
}

inline bool qFuzzyCompare(const QPointF &l, const QPointF &r) {
    return ::qFuzzyCompare(l.x(), r.x()) && ::qFuzzyCompare(l.y(), r.y());
}

inline bool qFuzzyCompare(const QSizeF &l, const QSizeF &r) {
    return ::qFuzzyCompare(l.width(), r.width()) && ::qFuzzyCompare(l.height(), r.height());
}

inline bool qFuzzyCompare(const QRectF &l, const QRectF &r) {
    return 
        ::qFuzzyCompare(l.x(), r.x()) && 
        ::qFuzzyCompare(l.y(), r.y()) && 
        ::qFuzzyCompare(l.width(), r.width()) &&
        ::qFuzzyCompare(l.height(), r.height());
}


#endif // QN_SCENE_UTILITY_H
