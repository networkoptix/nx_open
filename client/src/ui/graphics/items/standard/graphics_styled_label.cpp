#include "graphics_styled_label.h"

GraphicsStyledLabel::GraphicsStyledLabel(QGraphicsItem *parent, Qt::WindowFlags f) :
    GraphicsLabel(parent, f)
{
}

GraphicsStyledLabel::~GraphicsStyledLabel() {
}

void GraphicsStyledLabel::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {

    base_type::paint(painter, option, widget);
}
