#ifndef GRAPHICSFRAME_P_H
#define GRAPHICSFRAME_P_H

#include "graphicswidget_p.h"
#include "graphicsframe.h"

class GraphicsFramePrivate: public GraphicsWidgetPrivate
{
    Q_DECLARE_PUBLIC(GraphicsFrame)

public:
    GraphicsFramePrivate()
        : GraphicsWidgetPrivate(),
          frect(QRect(0, 0, 0, 0)),
          frameStyle(GraphicsFrame::NoFrame | GraphicsFrame::Plain),
          lineWidth(1),
          midLineWidth(0),
          frameWidth(0),
          leftFrameWidth(0), rightFrameWidth(0), topFrameWidth(0), bottomFrameWidth(0)
    {}

    void updateFrameWidth();
    void updateStyledFrameWidths();

    QRect frect;
    int frameStyle;
    short lineWidth;
    short midLineWidth;
    short frameWidth;
    short leftFrameWidth, rightFrameWidth, topFrameWidth, bottomFrameWidth;
};

#endif // GRAPHICSFRAME_P_H
