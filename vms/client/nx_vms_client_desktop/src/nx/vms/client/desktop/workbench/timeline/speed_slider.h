// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QTimer>
#include <QtWidgets/QSlider>

#include <ui/workbench/workbench_context_aware.h>

class VariantAnimator;

namespace nx::vms::client::desktop::workbench::timeline {

class SliderToolTip;

class SpeedSlider:
    public QSlider,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    Q_PROPERTY(qreal speed READ speed WRITE setSpeed)

    using base_type = QSlider;

public:
    explicit SpeedSlider(QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~SpeedSlider() override;

    void setTooltipsVisible(bool enabled);

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

    void hideToolTip();
    void showToolTip();

public slots:
    void finishAnimations();
    void speedUp();
    void speedDown();

signals:
    void speedChanged(qreal speed);
    void roundedSpeedChanged(qreal roundedSpeed);
    void wheelMoved(bool forwardDirection);

protected:
    virtual void sliderChange(SliderChange change) override;
    virtual void wheelEvent(QWheelEvent* event) override;
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

protected slots:
    void restartSpeedAnimation();
    void updateTooltipPos();
    virtual void enterEvent(QEnterEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;

private:
    bool calculateToolTipAutoVisibility() const;

private:
    qreal m_roundedSpeed = 0.0;
    qreal m_minimalSpeedStep = 1.0;
    qreal m_defaultSpeed = 1.0;

    bool m_tooltipsEnabled = false;

    VariantAnimator* const m_animator;

    QTimer m_hideTooltipTimer;
    std::unique_ptr<SliderToolTip> m_tooltip;
};

} // namespace nx::vms::client::desktop::workbench::timeline
