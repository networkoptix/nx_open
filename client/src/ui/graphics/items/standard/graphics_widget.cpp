#include "graphics_widget.h"
#include "graphics_widget_p.h"

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

GraphicsStyle *GraphicsWidget::style() const {
    Q_D(const GraphicsWidget);

    if(!d->style) {
        d->style = dynamic_cast<GraphicsStyle *>(base_type::style());

        if(!d->style) {
            if(!d->reserveStyle)
                d->reserveStyle.reset(new GraphicsStyle());

            d->reserveStyle->setBaseStyle(base_type::style());
            d->style = d->reserveStyle.data();
        }
    }

    return d->style;
}

void GraphicsWidget::initStyleOption(QStyleOption *option) const {
    base_type::initStyleOption(option);
}

void GraphicsWidget::setStyle(GraphicsStyle *style) {
    setStyle(style->baseStyle());
}

void GraphicsWidget::changeEvent(QEvent *event) {
    Q_D(GraphicsWidget);

    base_type::changeEvent(event);

    if(event->type() == QEvent::StyleChange)
        d->style = NULL;
}
