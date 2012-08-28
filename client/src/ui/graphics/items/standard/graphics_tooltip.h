#ifndef GRAPHICSTOOLTIP_H
#define GRAPHICSTOOLTIP_H

#include <QString>
#include <QtGui/QGraphicsItem>

class GraphicsTooltip{
public:
    static void showText(QString text, QGraphicsItem *item, QPointF pos, QRectF viewport);
    static inline void hideText() { showText(QString(), NULL, QPointF(), QRectF()); }
};



#endif //GRAPHICSTOOLTIP_H
