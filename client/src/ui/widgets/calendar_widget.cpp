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

    setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    setVerticalHeaderFormat(QnCalendarWidget::NoVerticalHeader);

    QTableView *table = findChild<QTableView *>(QLatin1String("qt_calendar_calendarview"));
    QHeaderView* header = table->horizontalHeader();
    header->setMinimumSectionSize(18);

    QWidget* navBarBackground = findChild<QWidget *>(QLatin1String("qt_calendar_navigationbar"));
    navBarBackground->setBackgroundRole(QPalette::Window);
}

void QnCalendarWidget::setCurrentTimePeriods( Qn::TimePeriodRole type, QnTimePeriodList periods )
{
    bool oldEmpty = isEmpty();
    m_currentTimeStorage.setPeriods(type, periods);
    bool newEmpty = isEmpty();
    if (newEmpty != oldEmpty)
        emit emptyChanged();

    update();
}

void QnCalendarWidget::setSyncedTimePeriods( Qn::TimePeriodRole type, QnTimePeriodList periods )
{
    bool oldEmpty = isEmpty();
    m_syncedTimeStorage.setPeriods(type, periods);
    bool newEmpty = isEmpty();
    if (newEmpty != oldEmpty)
        emit emptyChanged();

    update();
}

bool QnCalendarWidget::isEmpty(){
    for(int type = 0; type < Qn::TimePeriodRoleCount; type++)
        if (!m_currentTimeStorage.periods(static_cast<Qn::TimePeriodRole>(type)).empty()
                ||
            !m_syncedTimeStorage.periods(static_cast<Qn::TimePeriodRole>(type)).empty())
            return false;
    return true;
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
