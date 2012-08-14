#include "calendar_widget.h"

#include "utils/common/event_processors.h"

#include <QtGui/QPainter>
#include <QtGui/QTableView>
#include <QtGui/QHeaderView>

#define DAY 1000 * 60 * 60 * 24

QnCalendarWidget::QnCalendarWidget():
    QCalendarWidget()
{
    /* Month button's drop-down menu doesn't work well with graphics scene, so we simply remove it. */
    QToolButton *monthButton = findChild<QToolButton *>(QLatin1String("qt_calendar_monthbutton"));
    if(monthButton)
        monthButton->setMenu(NULL);

    QWidget *yearEdit = findChild<QWidget *>(QLatin1String("qt_calendar_yearedit"));
    QnSingleEventEater *contextMenuEater = new QnSingleEventEater(Qn::AcceptEvent, yearEdit);
    contextMenuEater->setEventType(QEvent::ContextMenu);
    yearEdit->installEventFilter(contextMenuEater);

    setHorizontalHeaderFormat(SingleLetterDayNames);

    QTableView *table = findChild<QTableView *>(QLatin1String("qt_calendar_calendarview"));
    QHeaderView* header = table->horizontalHeader();
    header->setMinimumSectionSize(18);
}

void QnCalendarWidget::setCurrentTimePeriods( Qn::TimePeriodRole type, QnTimePeriodList periods )
{
    m_currentTimeStorage.setPeriods(type, periods);

    int w = minimumSizeHint().width();
    qDebug() << w;
    update();
}

void QnCalendarWidget::setSyncedTimePeriods( Qn::TimePeriodRole type, QnTimePeriodList periods )
{
    m_syncedTimeStorage.setPeriods(type, periods);

    update();
}

void QnCalendarWidget::paintCell(QPainter *painter, const QRect & rect, const QDate & date ) const{
    QCalendarWidget::paintCell(painter, rect, date);

    if (date < this->minimumDate() || date > this->maximumDate())
        return;

    QDateTime dt(date);
    QnTimePeriod current(dt.toMSecsSinceEpoch(), DAY);

    QBrush brush = painter->brush();

    if (m_currentTimeStorage.periods(Qn::MotionRole).intersects(current)){
        brush.setColor(QColor(255, 0, 0, 70));
        brush.setStyle(Qt::SolidPattern);
    }
    else if (m_currentTimeStorage.periods(Qn::RecordingRole).intersects(current)){
        brush.setColor(QColor(0, 255, 0, 70));
        brush.setStyle(Qt::SolidPattern);
    } 
    else if (m_syncedTimeStorage.periods(Qn::MotionRole).intersects(current)){
        brush.setColor(QColor(255, 0, 0, 70));
        brush.setStyle(Qt::BDiagPattern);
    }
    else if (m_syncedTimeStorage.periods(Qn::RecordingRole).intersects(current)){
        brush.setColor(QColor(0, 255, 0, 70));
        brush.setStyle(Qt::BDiagPattern);
    } 
    else {
        brush.setColor(QColor(127, 127, 127, 177));
        brush.setStyle(Qt::SolidPattern);
    }
    painter->fillRect(rect, brush);
}
