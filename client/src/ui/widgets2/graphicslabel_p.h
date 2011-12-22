#ifndef QN_GRAPHICS_LABEL_P_H
#define QN_GRAPHICS_LABEL_P_H

#include "graphicsframe_p.h"

class QGraphicsSimpleTextItem;

class GraphicsLabelPrivate : public GraphicsFramePrivate
{
    Q_DECLARE_PUBLIC(GraphicsLabel)
public:
    GraphicsLabelPrivate() {}

    void init();

    void updateTextBrush();
    void updateTextFont();

    QGraphicsSimpleTextItem *textItem;
};

#endif // QN_GRAPHICS_LABEL_P_H
