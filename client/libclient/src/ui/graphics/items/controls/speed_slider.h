#ifndef QN_SPEED_SLIDER_H
#define QN_SPEED_SLIDER_H

#include <QtCore/QBasicTimer>

#include <ui/graphics/items/generic/tool_tip_slider.h>
#include <ui/animation/animated.h>

class VariantAnimator;

class QnSpeedSlider: public Animated<QnToolTipSlider> {
    Q_OBJECT;
    Q_PROPERTY(qreal speed READ speed WRITE setSpeed);

    typedef Animated<QnToolTipSlider> base_type;

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

    Q_SLOT void finishAnimations();

signals:
    void speedChanged(qreal speed);
    void roundedSpeedChanged(qreal roundedSpeed);

public slots:
    void speedUp();
    void speedDown();

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual void timerEvent(QTimerEvent *event) override;
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event) override;

protected slots:
    void restartSpeedAnimation();

private:
    qreal m_roundedSpeed;
    qreal m_minimalSpeedStep;
    qreal m_defaultSpeed;
    VariantAnimator *m_animator;
    QBasicTimer m_wheelStuckTimer;
    bool m_wheelStuck;
};

#endif // QN_SPEED_SLIDER_H
