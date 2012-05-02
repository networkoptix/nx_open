#ifndef QN_SPEED_SLIDER_H
#define QN_SPEED_SLIDER_H

#include "tool_tip_slider.h"

class QPropertyAnimation;

class QnToolTipItem;

class QnSpeedSlider: public QnToolTipSlider {
    Q_OBJECT;

    typedef QnToolTipSlider base_type;

public:
    explicit QnSpeedSlider(QGraphicsItem *parent = NULL);
    virtual ~QnSpeedSlider();

    enum Precision { LowPrecision, HighPrecision };

    Precision precision() const;
    void setPrecision(Precision precision);

public slots:
    void resetSpeed();
    void setSpeed(float value);

    void speedDown();
    void speedUp();

signals:
    void speedChanged(float newSpeed);

    void frameBackward();
    void frameForward();

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void timerEvent(QTimerEvent *event) override;
    virtual void wheelEvent(QGraphicsSceneWheelEvent *e) override;

private:
    Q_DISABLE_COPY(QnSpeedSlider)

    Precision m_precision;
    QPropertyAnimation *m_animation;
    int m_wheelStuckedTimerId;
    bool m_wheelStucked;
};

#endif // QN_SPEED_SLIDER_H
