#ifndef QN_SPEED_SLIDER_H
#define QN_SPEED_SLIDER_H

#include "tool_tip_slider.h"

class QPropertyAnimation;

class QnSpeedSlider: public QnToolTipSlider {
    Q_OBJECT;
    Q_PROPERTY(qreal speed READ speed WRITE setSpeed);

    typedef QnToolTipSlider base_type;

public:
    enum Precision { 
        LowPrecision, 
        HighPrecision 
    };

    explicit QnSpeedSlider(QGraphicsItem *parent = NULL);
    virtual ~QnSpeedSlider();

    Precision precision() const;
    void setPrecision(Precision precision);

    qreal speed() const;

public slots:
    void resetSpeed();
    void setSpeed(qreal value);

    void speedDown();
    void speedUp();

signals:
    void speedChanged(qreal newSpeed);

    void frameBackward();
    void frameForward();

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    virtual void timerEvent(QTimerEvent *event) override;
    virtual void wheelEvent(QGraphicsSceneWheelEvent *e) override;

private:
    Precision m_precision;
    QPropertyAnimation *m_animation;
    int m_wheelStuckedTimerId;
    bool m_wheelStucked;
};

#endif // QN_SPEED_SLIDER_H
