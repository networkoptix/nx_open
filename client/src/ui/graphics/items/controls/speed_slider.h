#ifndef QN_SPEED_SLIDER_H
#define QN_SPEED_SLIDER_H

#include "tool_tip_slider.h"

class QPropertyAnimation;

class QnSpeedSlider: public QnToolTipSlider {
    Q_OBJECT;
    Q_PROPERTY(qreal speed READ speed WRITE setSpeed);

    typedef QnToolTipSlider base_type;

public:
    explicit QnSpeedSlider(QGraphicsItem *parent = NULL);
    virtual ~QnSpeedSlider();

    qreal speed() const;
    void setSpeed(qreal speed);
    void resetSpeed();
    qreal roundedSpeed() const;

    qreal minimalSpeed() const;
    void setMinimalSpeed(qreal minimalSpeed);

    qreal maximalSpeed() const;
    void setMaximalSpeed(qreal maximalSpeed);

    void setSpeedRange(qreal minimalSpeed, qreal maximalSpeed);

    qreal defaultSpeed() const;
    void setDefaultSpeed(qreal defaultSpeed);

    qreal minimalSpeedStep() const;
    void setMinimalSpeedStep(qreal minimalSpeedStep);

signals:
    void speedChanged(qreal newSpeed);

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void timerEvent(QTimerEvent *event) override;
    virtual void wheelEvent(QGraphicsSceneWheelEvent *e) override;

private:
    qreal m_minimalSpeedStep;
    qreal m_defaultSpeed;
    QPropertyAnimation *m_animation;
    int m_wheelStuckedTimerId;
    bool m_wheelStucked;
};

#endif // QN_SPEED_SLIDER_H
