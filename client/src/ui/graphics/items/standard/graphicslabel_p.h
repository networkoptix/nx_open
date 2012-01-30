#ifndef GRAPHICSLABEL_P_H
#define GRAPHICSLABEL_P_H

#include "graphicsframe_p.h"

class QGraphicsSimpleTextItem;

class GraphicsLabelPrivate : public GraphicsFramePrivate
{
    Q_DECLARE_PUBLIC(GraphicsLabel)

public:
    void init();

    void updateTextBrush();
    void updateTextFont();

    QGraphicsSimpleTextItem *textItem;
};

#endif // GRAPHICSLABEL_P_H
