#ifndef QN_INSTRUMENT_UTILITY_H
#define QN_INSTRUMENT_UTILITY_H

#include <Qt>

class QGraphicsView;
class QRect;
class QRectF;
class QSizeF;
class QPoint;
class QPointF;
class QTransform;

class InstrumentUtility {
public:
    enum BoundingMode {
        InBound,    /**< Rectangle's size limit is reached when it is contained inside a bounding rectangle touching its sides. */
        OutBound    /**< Rectangle's size limit is reached when bounding rectangle is contained inside it touching its sides. */
    };

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
     * Bounds the given size, so that a rectangle of size maxSize would include
     * a rectangle of given size.
     *
     * Note that <tt>Qt::KeepAspectRatio</tt> and <tt>Qt::KeepAspectRatioByExpanding</tt>
     * modes are not distinguished by this function.
     *
     * \param size                      Size to bound.
     * \param maxSize                   Maximal size. 
     * \param mode                      Bounding mode. 
     */
    static QSizeF bounded(const QSizeF &size, const QSizeF &maxSize, Qt::AspectRatioMode mode);

    /**
     * Expands the given size, so that a rectangle of given size would include
     * a rectangle of size minSize.
     * 
     * Note that <tt>Qt::KeepAspectRatio</tt> and <tt>Qt::KeepAspectRatioByExpanding</tt>
     * modes are not distinguished by this function.
     *
     * \param size                      Size to expand.
     * \param maxSize                   Minimal size. 
     * \param mode                      Bounding mode. 
     */
    static QSizeF expanded(const QSizeF &size, const QSizeF &minSize, Qt::AspectRatioMode mode);

    /**
     * Dilates the given rectangle by the given amount.
     * 
     * \param rect                      Rectangle to dilate.
     * \param amount                    Dilation amount.
     * \returns                         Dilated rectangle.
     */
    static QRectF dilated(const QRectF &rect, const QSizeF &amount);

    /**
     * Moves the given viewport.
     * 
     * \param view                      Graphics view to move viewport of.
     * \param positionDelta             Move delta, in viewport coordinates.
     */
    static void moveViewport(QGraphicsView *view, const QPoint &positionDelta);

    /**
     * Moves the given viewport.
     * 
     * \param view                      Graphics view to move viewport of.
     * \param sceneToViewport           Current scene-to-viewport transform of
     *                                  the given graphics view.
     * \param positionDelta             Move delta, in scene coordinates.
     */
    static void moveViewport(QGraphicsView *view, const QTransform &sceneToViewport, const QPointF &positionDelta);

    /**
     * Moves the given viewport.
     * 
     * \param view                      Graphics view to move viewport of.
     * \param positionDelta             Move delta, in scene coordinates.
     */
    static void moveViewport(QGraphicsView *view, const QPointF &positionDelta);

    /**
     * Scales the given viewport.
     * 
     * Note that because of the way scaling is implemented, it may also
     * involve minor viewport movement.
     * 
     * \param view                      Graphics view to scale viewport of.
     * \param factor                    Viewport scale factor.
     */
    static void scaleViewport(QGraphicsView *view, qreal factor);

    /**
     * Calculates the factor by which the given size must be scaled to fit into
     * the given bounds.
     * 
     * \param size                      Size to scale.
     * \param bounds                    Size bounds.
     * \param mode                      Bounding mode.
     */
    static qreal calculateScale(QSizeF size, QSizeF bounds, BoundingMode mode);

};

#endif // QN_INSTRUMENT_UTILITY_H
