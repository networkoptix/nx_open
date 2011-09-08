#ifndef SPEEDSLIDER_H
#define SPEEDSLIDER_H

#include "graphicsslider.h"

class QPropertyAnimation;

class SpeedSlider : public GraphicsSlider
{
    Q_OBJECT

public:
    explicit SpeedSlider(Qt::Orientation orientation = Qt::Horizontal, QGraphicsItem *parent = 0);
    ~SpeedSlider();

    enum Precision { LowPrecision, HighPrecision };

    Precision precision() const;
    void setPrecision(Precision precision);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

public Q_SLOTS:
    void resetSpeed();

    void stepBackward();
    void stepForward();

Q_SIGNALS:
    void speedChanged(float newSpeed);

protected:
    void sliderChange(SliderChange change);

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

private:
    Q_DISABLE_COPY(SpeedSlider)

    Precision m_precision;
    QPropertyAnimation *m_animation;
};

#endif // SPEEDSLIDER_H
