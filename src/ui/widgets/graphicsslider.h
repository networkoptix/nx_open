#ifndef GRAPHICSSLIDER_H
#define GRAPHICSSLIDER_H

#include "abstractgraphicsslider.h"

class GraphicsSliderPrivate;
class GraphicsSlider : public AbstractGraphicsSlider
{
    Q_OBJECT

    Q_ENUMS(TickPosition)
    Q_PROPERTY(TickPosition tickPosition READ tickPosition WRITE setTickPosition)
    Q_PROPERTY(int tickInterval READ tickInterval WRITE setTickInterval)

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

    QSize sizeHint() const;
    QSize minimumSizeHint() const;

    void setTickPosition(TickPosition position);
    TickPosition tickPosition() const;

    void setTickInterval(int ti);
    int tickInterval() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

protected:
    void initStyleOption(QStyleOptionSlider *option) const;

    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    bool event(QEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

private:
    Q_DISABLE_COPY(GraphicsSlider)
    Q_DECLARE_PRIVATE(GraphicsSlider)
};

#endif // GRAPHICSSLIDER_H
