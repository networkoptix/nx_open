#ifndef TIMESLIDER_H
#define TIMESLIDER_H

#include "ui/widgets2/graphicswidget.h"

class QPropertyAnimation;

class MySlider;
class TimeLine;
class ToolTipItem;

class TimeSlider : public GraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(qint64 currentValue READ currentValue WRITE setCurrentValue NOTIFY currentValueChanged)
    Q_PROPERTY(qint64 maximumValue READ maximumValue WRITE setMaximumValue NOTIFY maximumValueChanged)
    Q_PROPERTY(float scalingFactor READ scalingFactor WRITE setScalingFactor NOTIFY scalingFactorChanged)
    Q_PROPERTY(qint64 viewPortPos READ viewPortPos WRITE setViewPortPos)

public:
    explicit TimeSlider(QGraphicsItem *parent = 0);
    ~TimeSlider();

    void setToolTipItem(ToolTipItem *toolTip);

    qint64 length() const;
    qint64 minimumValue() const;
    qint64 maximumValue() const;
    qint64 currentValue() const;
    float scalingFactor() const;

    bool isMoving() const;
    void setMoving(bool b);

    bool centralise() const { return m_centralise; }
    void setCentralise(bool b) { m_centralise = b; }

    qint64 sliderRange();

    qint64 minimumRange() const;
    void setMinimumRange(qint64);

public Q_SLOTS:
    void setMinimumValue(qint64 value);
    void setMaximumValue(qint64 value);
    void setCurrentValue(qint64 value);
    void setScalingFactor(float factor);

    inline void zoomIn()
    { setScalingFactor(scalingFactor() + 1); }
    inline void zoomOut()
    { setScalingFactor(scalingFactor() - 1); }

Q_SIGNALS:
    void minimumValueChanged(qint64 value);
    void maximumValueChanged(qint64 value);
    void currentValueChanged(qint64 value);
    void scalingFactorChanged(qint64 value);
    void sliderPressed();
    void sliderReleased();

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    bool eventFilter(QObject *target, QEvent *event);

private Q_SLOTS:
    void onSliderValueChanged(int value);
    void centraliseSlider();
    void onWheelAnimationFinished();

private:
    float delta() const;

    qint64 fromSlider(int value);
    int toSlider(qint64 value);

    qint64 viewPortPos() const;
    void setViewPortPos(qint64 value);

    void updateSlider();

private:
    MySlider *m_slider;
    TimeLine *m_timeLine;

    qint64 m_minimumValue;
    qint64 m_maximumValue;
    qint64 m_currentValue;
    qint64 m_viewPortPos;

    float m_scalingFactor;
    bool m_isUserInput;
    int m_delta;

    QPropertyAnimation *m_animation;
    bool m_centralise;
    qint64 m_minimumRange;


    friend class TimeLine;
};

#endif // TIMESLIDER_H
