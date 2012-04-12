#ifndef QN_TIME_SLIDER_H
#define QN_TIME_SLIDER_H

#include "tool_tip_slider.h"
#include <recording/time_period.h>

class QnNoptixStyle;

class QnTimeSlider: public QnToolTipSlider {
    Q_OBJECT;

    typedef QnToolTipSlider base_type;

public:
    enum DisplayLine {
        SelectionLine,
        LayoutLine,
        LineCount
    };

    enum PeriodType {
        RecordingPeriod,
        MotionPeriod,
        PeriodTypeCount
    };

    enum Option {
        StickToMinimum = 0x1,
        StickToMaximum = 0x2
    };
    Q_DECLARE_FLAGS(Options, Option);

    explicit QnTimeSlider(QGraphicsItem *parent = NULL);
    virtual ~QnTimeSlider();

    QnTimePeriodList timePeriods(DisplayLine line, PeriodType type) const;
    void setTimePeriods(DisplayLine line, PeriodType type, const QnTimePeriodList &timePeriods);

    Options options() const;
    void setOptions(Options options);

    qint64 windowStart() const;
    void setWindowStart(qint64 windowStart);

    qint64 windowEnd() const;
    void setWindowEnd(qint64 windowEnd);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    virtual QPointF positionFromValue(qint64 logicalValue) const override;
    virtual qint64 valueFromPosition(const QPointF &position) const override;

protected:
    virtual void sliderChange(SliderChange change) override;

protected slots:
    void at_actionTriggered(int action);

private:
    void drawPeriodsBar(QPainter *painter, QnTimePeriodList &recorded, QnTimePeriodList &motion, qreal top, qreal height);
    void drawPeriods(QPainter *painter, QnTimePeriodList &periods, qreal top, qreal height, const QColor &preColor, const QColor &pastColor);

    void updateToolTipVisibility();

private:
    Q_DECLARE_PRIVATE(GraphicsSlider);

    QnTimePeriodList m_timePeriods[LineCount][PeriodTypeCount];
    qint64 m_windowStart, m_windowEnd;
    qint64 m_oldMinimum, m_oldMaximum;
    Options m_options;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnTimeSlider::Options);

#endif // QN_TIME_SLIDER_H
