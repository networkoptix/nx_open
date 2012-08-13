#include "calendar_widget.h"

#define DAY 1000 * 60 * 60 * 24

QnCalendarWidget::QnCalendarWidget(): 
    QCalendarWidget()
{
    m_currentTimeStorage.setAggregationMSecs(DAY);
    m_syncedTimeStorage.setAggregationMSecs(DAY);
}

void QnCalendarWidget::setCurrentTimePeriods( Qn::TimePeriodRole type, QnTimePeriodList periods )
{
    m_currentTimeStorage.updatePeriods(type, periods);

    update();
}

void QnCalendarWidget::setSyncedTimePeriods( Qn::TimePeriodRole type, QnTimePeriodList periods )
{
    m_syncedTimeStorage.updatePeriods(type, periods);

    update();
}

void QnCalendarWidget::paintCell(QPainter *painter, const QRect & rect, const QDate & date ) const{
    QCalendarWidget::paintCell(painter, rect, date);

    if (date < this->minimumDate() || date > this->maximumDate())
        return;

    QDateTime dt(date);
    QnTimePeriod current(dt.toMSecsSinceEpoch(), DAY);

    QBrush brush = painter->brush();

    if (m_currentTimeStorage.aggregated(Qn::MotionRole).intersects(current)){
        brush.setColor(QColor(255, 0, 0, 70));
        brush.setStyle(Qt::SolidPattern);
    }
    else if (m_currentTimeStorage.aggregated(Qn::RecordingRole).intersects(current)){
        brush.setColor(QColor(0, 255, 0, 70));
        brush.setStyle(Qt::SolidPattern);
    } 
    else if (m_syncedTimeStorage.aggregated(Qn::MotionRole).intersects(current)){
        brush.setColor(QColor(255, 0, 0, 70));
        brush.setStyle(Qt::BDiagPattern);
    }
    else if (m_syncedTimeStorage.aggregated(Qn::RecordingRole).intersects(current)){
        brush.setColor(QColor(0, 255, 0, 70));
        brush.setStyle(Qt::BDiagPattern);
    } 
    else {
        brush.setColor(QColor(127, 127, 127, 177));
        brush.setStyle(Qt::SolidPattern);
    }
    painter->fillRect(rect, brush);
}