#ifndef GRAPHICS_STYLED_LABEL_H
#define GRAPHICS_STYLED_LABEL_H

#include <ui/graphics/items/standard/graphics_label.h>

class GraphicsStyledLabel : public GraphicsLabel
{
    Q_OBJECT

    typedef GraphicsLabel base_type;
public:
    explicit GraphicsStyledLabel(QGraphicsItem *parent = 0, Qt::WindowFlags f = 0);
    ~GraphicsStyledLabel();

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

};

#endif // GRAPHICS_STYLED_LABEL_H
