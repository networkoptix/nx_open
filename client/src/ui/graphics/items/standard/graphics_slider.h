#ifndef GRAPHICSSLIDER_H
#define GRAPHICSSLIDER_H

#include "abstract_linear_graphics_slider.h"

class GraphicsSliderPrivate;
class GraphicsSliderPositionConverter;

class GraphicsSlider : public AbstractLinearGraphicsSlider
{
    Q_OBJECT

    Q_ENUMS(TickPosition)
    Q_PROPERTY(TickPosition tickPosition READ tickPosition WRITE setTickPosition)
    Q_PROPERTY(qint64 tickInterval READ tickInterval WRITE setTickInterval)

    typedef AbstractLinearGraphicsSlider base_type;

public:
    enum TickPosition {
        NoTicks = 0,
        TicksAbove = 1,
        TicksLeft = TicksAbove,
        TicksBelow = 2,
        TicksRight = TicksBelow,
        TicksBothSides = 3
    };

    explicit GraphicsSlider(QGraphicsItem *parent = 0);
    explicit GraphicsSlider(Qt::Orientation orientation, QGraphicsItem *parent = 0);
    virtual ~GraphicsSlider();

    void setTickPosition(TickPosition position);
    TickPosition tickPosition() const;

    void setTickInterval(qint64 tickInterval);
    qint64 tickInterval() const;

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    virtual void initStyleOption(QStyleOption *option) const override;

    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;

    virtual bool event(QEvent *event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

protected:
    GraphicsSlider(GraphicsSliderPrivate &dd, QGraphicsItem *parent);

    friend class PositionValueConverter;

private:
    Q_DISABLE_COPY(GraphicsSlider)
    Q_DECLARE_PRIVATE(GraphicsSlider)
};


#endif // GRAPHICSSLIDER_H
