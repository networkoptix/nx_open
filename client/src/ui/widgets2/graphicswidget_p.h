#ifndef GRAPHICS_WIDGET_PRIVATE_H
#define GRAPHICS_WIDGET_PRIVATE_H

#include "graphicswidget.h"

class GraphicsWidgetPrivate
{
    Q_DECLARE_PUBLIC(GraphicsWidget)

public:
    GraphicsWidgetPrivate() :
        q_ptr(0),
        isUnderMouse(false)
    {}

    virtual ~GraphicsWidgetPrivate() {}

protected:
    GraphicsWidget *q_ptr;

private:
    /** QTBUG-18797: When setting the flag ItemIgnoresTransformations for an item,
     * it will receive mouse events as if it was transformed by the view. */
    uint isUnderMouse : 1;
    uint reserved : 31;
};

#endif // GRAPHICSWIDGET_P_H
