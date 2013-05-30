#ifndef QN_GRAPHICS_TOOL_TIP_H
#define QN_GRAPHICS_TOOL_TIP_H

#include <QtCore/QString>
#include <QtGui/QGraphicsItem>
#include <QtGui/QGraphicsView>

class GraphicsToolTip {
public:
    /**
     * @brief showText      Show tooltip on the scene.
     * @param text          Text of the tooltip.
     * @param view          Graphics view owning the tooltip.
     * @param item          Graphics item that provides the tip.
     * @param pos           Position of the mouse cursor in viewport coordinates.
     */
    static void showText(const QString &text, QGraphicsView *view, QGraphicsItem *item, const QPoint &pos);

    /**
     * @brief hideText      Hide displayed tooltip (if any).
     */
    static inline void hideText() { showText(QString(), NULL, QPointF(), QRectF()); }

private:
    static void showText(const QString &text, QGraphicsItem *item, QPointF scenePos, QRectF sceneRect);
};


#endif // QN_GRAPHICS_TOOL_TIP_H
