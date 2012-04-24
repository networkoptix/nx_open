#ifndef GRAPHICSLABEL_P_H
#define GRAPHICSLABEL_P_H

#include "graphics_frame_p.h"
#include "graphics_label.h"

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
