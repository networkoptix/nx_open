#include "graphicswidget.h"
#include "graphicswidget_p.h"

GraphicsWidget::GraphicsWidget(QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    QGraphicsWidget(parent, windowFlags),
    d_ptr(new GraphicsWidgetPrivate)
{
    d_ptr->q_ptr = this;
}

GraphicsWidget::GraphicsWidget(GraphicsWidgetPrivate &dd, QGraphicsItem *parent, Qt::WindowFlags windowFlags):
    QGraphicsWidget(parent, windowFlags),
    d_ptr(&dd)
{
    d_ptr->q_ptr = this;
}

GraphicsWidget::~GraphicsWidget() {
    return;
}

