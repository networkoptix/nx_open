#ifndef QN_GRAPHICS_TOOLTIP_H
#define QN_GRAPHICS_TOOLTIP_H

#include <QtCore/QString>
#include <QtGui/QGraphicsItem>

class GraphicsTooltip {
public:
    // TODO: #GDM add doxydocs here --- what are the coordinates passed, in which coordinate system (scene, viewport, item?).
    // 
    // Passing viewport as QRectF is not a good idea as scene<->view transformation is, generally speaking, an unrestricted homography.
    // I think we should just pass the graphics view, defaulted to NULL.

    static void showText(QString text, QGraphicsItem *item, QPointF pos, QRectF viewport);
    static inline void hideText() { showText(QString(), NULL, QPointF(), QRectF()); }
};

#endif // QN_GRAPHICS_TOOLTIP_H
