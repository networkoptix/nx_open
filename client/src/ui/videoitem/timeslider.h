#ifndef TIMESLIDER_H
#define TIMESLIDER_H

#include <QtGui/QGraphicsWidget>

#include "ui/widgets2/graphicsslider.h"

class QPropertyAnimation;

class TimeLine;
class ToolTipItem;

class MySlider : public GraphicsSlider
{
    friend class TimeSlider; // ### for sizeHint()

    Q_OBJECT

public:
    MySlider(QGraphicsItem *parent = 0);

    // ### tmp
    inline int width() const { return rect().width(); }
    inline int height() const { return rect().height(); }
    //

    void setToolTipItem(ToolTipItem *toolTip);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private Q_SLOTS:
    void onValueChanged(int);

private:
    ToolTipItem *m_toolTip;
};


class TimeSlider : public QGraphicsWidget
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

    // TODO: use min and max, not length
    qint64 minimumValue() const;
    qint64 maximumValue() const;
    qint64 currentValue() const;
    float scalingFactor() const;

    bool isMoving() const;
    void setMoving(bool b);

    bool centralise() const { return m_centralise; }
    void setCentralise(bool b) { m_centralise = b; }

    int sliderValue() const;
    int sliderLength() const;
    qint64 viewPortPos() const;
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
    void currentValueChanged(qint64 value);
    void maximumValueChanged(qint64 value);
    void scalingFactorChanged(qint64 value);
    void sliderPressed();
    void sliderReleased();

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    bool eventFilter(QObject *target, QEvent *event);

private Q_SLOTS:
    void setViewPortPos(qint64 value);
    void onSliderPressed();
    void onSliderReleased();
    void onSliderValueChanged(int value);
    void onWheelAnimationFinished();

private:
    float delta() const;

    qint64 fromSlider(int value);
    int toSlider(qint64 value);

    void updateSlider();
    void centraliseSlider();

private:
    MySlider *m_slider;
    TimeLine *m_timeLine;

    qint64 m_minimumValue;
    qint64 m_maximumValue;
    qint64 m_currentValue;
    qint64 m_viewPortPos;

    float m_scalingFactor;
    bool m_isUserInput;
    bool m_sliderPressed;
    int m_delta;

    QPropertyAnimation *m_animation;
    bool m_centralise;
    qint64 m_minimumRange;


    friend class TimeLine;
};

#endif // TIMESLIDER_H
