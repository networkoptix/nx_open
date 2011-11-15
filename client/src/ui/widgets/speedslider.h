#ifndef SPEEDSLIDER_H
#define SPEEDSLIDER_H

#include "ui/widgets2/graphicsslider.h"

class QPropertyAnimation;

class ToolTipItem;

class SpeedSlider : public GraphicsSlider
{
    Q_OBJECT

public:
    explicit SpeedSlider(Qt::Orientation orientation = Qt::Horizontal, QGraphicsItem *parent = 0);
    ~SpeedSlider();

    enum Precision { LowPrecision, HighPrecision };

    Precision precision() const;
    void setPrecision(Precision precision);

    void setToolTipItem(ToolTipItem *toolTip);

public Q_SLOTS:
    void resetSpeed();

    void stepBackward();
    void stepForward();
    void onLiveMode(bool value);
Q_SIGNALS:
    void speedChanged(float newSpeed);

    void frameBackward();
    void frameForward();

protected:
    void sliderChange(SliderChange change);

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
#ifndef QT_NO_WHEELEVENT
    void wheelEvent(QGraphicsSceneWheelEvent *e);
#endif
    void timerEvent(QTimerEvent *event);

private Q_SLOTS:
    void onSpeedChanged(float newSpeed);

private:
    Q_DISABLE_COPY(SpeedSlider)

    Precision m_precision;
    QPropertyAnimation *m_animation;
    ToolTipItem *m_toolTip;
    int m_toolTipTimerId;
    int m_wheelStuckedTimerId;
    bool m_wheelStucked;
};

#endif // SPEEDSLIDER_H
