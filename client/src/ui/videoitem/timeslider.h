#ifndef TIMESLIDER_H
#define TIMESLIDER_H

#include "ui/widgets2/graphicswidget.h"
#include "recording/time_period.h"

class QPropertyAnimation;

class MySlider;
class TimeLine;
class ToolTipItem;

class TimeSlider : public GraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(qint64 currentValue READ currentValue WRITE setCurrentValue NOTIFY currentValueChanged)
    Q_PROPERTY(qint64 maximumValue READ maximumValue WRITE setMaximumValue NOTIFY maximumValueChanged)
    Q_PROPERTY(double scalingFactor READ scalingFactor WRITE setScalingFactor NOTIFY scalingFactorChanged)
    Q_PROPERTY(qint64 viewPortPos READ viewPortPos WRITE setViewPortPos)

public:
    explicit TimeSlider(QGraphicsItem *parent = 0);
    ~TimeSlider();

    ToolTipItem *toolTipItem() const;
    void setToolTipItem(ToolTipItem *toolTip);

    qint64 length() const;
    qint64 minimumValue() const;
    qint64 maximumValue() const;
    qint64 currentValue() const;
    double scalingFactor() const;

    bool isMoving() const;
    void setMoving(bool b);

    bool centralise() const { return m_centralise; }
    void setCentralise(bool b) { m_centralise = b; }

    qint64 sliderRange() const;
    qint64 viewPortPos() const;

    qint64 minimumRange() const;
    void setMinimumRange(qint64);

    QPair<qint64, qint64> selectionRange() const;
    void setSelectionRange(const QPair<qint64, qint64> &range);
    inline void setSelectionRange(qint64 begin, qint64 end)
    { setSelectionRange(QPair<qint64, qint64>(begin, end)); }
    inline void resetSelectionRange()
    { setSelectionRange(0, 0); }

    const QnTimePeriodList &timePeriodList() const { return m_timePeriodList; }
    void setTimePeriodList(const QnTimePeriodList &timePeriodList) { m_timePeriodList = timePeriodList; }

    bool isAtEnd();
    void setEndSize(qreal size);

public Q_SLOTS:
    void setMinimumValue(qint64 value);
    void setMaximumValue(qint64 value);
    void setCurrentValue(qint64 value);
    void setScalingFactor(double factor);

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

    void exportRange(qint64 begin, qint64 end);

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;

    virtual bool eventFilter(QObject *target, QEvent *event) override;
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private Q_SLOTS:
    void onSliderValueChanged(int value);
    void centraliseSlider();
    void onWheelAnimationFinished();

private:
    double delta() const;

    qint64 fromSlider(int value) const;
    int toSlider(qint64 value) const;

    void setViewPortPos(qint64 value);

    void updateSlider();

private:
    MySlider *m_slider;
    TimeLine *m_timeLine;

    qint64 m_minimumValue;
    qint64 m_maximumValue;
    qint64 m_currentValue;
    qint64 m_viewPortPos;
    qint64 m_minimumRange;
    double m_scalingFactor;
    int m_delta;

    bool m_isUserInput;

    QPropertyAnimation *m_animation;
    bool m_centralise;

    QnTimePeriodList m_timePeriodList;

    qreal m_endSize;

    friend class TimeLine;
    friend class MySlider;
};

#endif // TIMESLIDER_H
