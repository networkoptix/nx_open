#ifndef QN_GRAPHICS_WIDGET_P_H
#define QN_GRAPHICS_WIDGET_P_H

#include "graphics_widget.h"

class GraphicsStyle;

class GraphicsWidgetPrivate {
public:
    GraphicsWidgetPrivate(): q_ptr(NULL), style(NULL) {};
    virtual ~GraphicsWidgetPrivate() {}

protected:
    GraphicsWidget *q_ptr;
    mutable GraphicsStyle *style;
    mutable QScopedPointer<GraphicsStyle> reserveStyle;

private:
    Q_DECLARE_PUBLIC(GraphicsWidget);
};


#endif // QN_GRAPHICS_WIDGET_P_H
