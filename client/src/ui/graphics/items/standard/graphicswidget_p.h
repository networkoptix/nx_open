#ifndef QN_GRAPHICS_WIDGET_P_H
#define QN_GRAPHICS_WIDGET_P_H

#include "graphicswidget.h"

class GraphicsWidgetPrivate {
public:
    GraphicsWidgetPrivate(): q_ptr(NULL) {};

protected:
    GraphicsWidget *q_ptr;

private:
    Q_DECLARE_PUBLIC(GraphicsWidget);
};


#endif // QN_GRAPHICS_WIDGET_P_H
