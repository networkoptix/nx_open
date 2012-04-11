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

    explicit QnTimeSlider(QGraphicsItem *parent = NULL);
    virtual ~QnTimeSlider();

    QnTimePeriodList timePeriods(DisplayLine line, PeriodType type) const;
    void setTimePeriods(DisplayLine line, PeriodType type, const QnTimePeriodList &timePeriods);

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    QnTimePeriodList m_timePeriods[LineCount][PeriodTypeCount];
};


#endif // QN_TIME_SLIDER_H
