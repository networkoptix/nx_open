#ifndef GRAPHICSLABEL_P_H
#define GRAPHICSLABEL_P_H

#include "graphicsframe_p.h"
#include "graphicslabel.h"

class QGraphicsSimpleTextItem;

class GraphicsLabelPrivate : public GraphicsFramePrivate
{
    Q_DECLARE_PUBLIC(GraphicsLabel)

public:
    void init();

    void updateCachedData();

    QStaticText::PerformanceHint performanceHint;
    QString text;
    QStaticText staticText;
    QRectF rect;
};

#endif // GRAPHICSLABEL_P_H
