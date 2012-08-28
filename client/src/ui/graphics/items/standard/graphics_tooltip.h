#ifndef GRAPHICSTOOLTIP_H
#define GRAPHICSTOOLTIP_H

#include <QString>
#include <QtGui/QGraphicsItem>

class GraphicsTooltip{
public:
    static void showText(QString text, QGraphicsItem *item);
    static inline void hideText() { showText(QString(), NULL); }
};



#endif //GRAPHICSTOOLTIP_H
