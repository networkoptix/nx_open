#include "calendar_widget.h"

#include "utils/common/event_processors.h"
#include "utils/common/scoped_painter_rollback.h"

#include <QtGui/QPainter>
#include <QtGui/QTableView>
#include <QtGui/QHeaderView>

#define DAY 1000 * 60 * 60 * 24

QnCalendarWidget::QnCalendarWidget():
    QCalendarWidget(),
    m_tableView(0)
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

    m_tableView = findChild<QTableView *>(QLatin1String("qt_calendar_calendarview"));
    Q_ASSERT(m_tableView);
    m_tableView->horizontalHeader()->setMinimumSectionSize(18);
    connect(m_tableView, SIGNAL(changeDate(const QDate&, bool)), SLOT(dateChanged(const QDate&)));

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

void QnCalendarWidget::setSelectedWindow(quint64 windowStart, quint64 windowEnd){

    bool modified = false;
    if (windowStart != (quint64)m_window.startTimeMs){
        modified = windowStart/DAY != (quint64)m_window.startTimeMs/DAY;
    }
    if (!modified && windowEnd != (quint64)m_window.endTimeMs()){
        modified = windowEnd/DAY !=(quint64)m_window.endTimeMs()/DAY;
    }

    m_window.startTimeMs = windowStart;
    m_window.durationMs = windowEnd - windowStart;

    if (modified)
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
    QDateTime dt(date);
    QnTimePeriod current(dt.toMSecsSinceEpoch(), DAY);

    QnScopedPainterBrushRollback brushRollback(painter);
    Q_UNUSED(brushRollback);

    QBrush brush = painter->brush();

    if (m_currentTimeStorage.periods(Qn::MotionRole).intersects(current)){
        brush.setColor(QColor(255, 0, 0));
        brush.setStyle(Qt::SolidPattern);
    }
    else if (m_currentTimeStorage.periods(Qn::RecordingRole).intersects(current)){
        brush.setColor(QColor(0, 220, 0));
        brush.setStyle(Qt::SolidPattern);
    } 
    else {
        brush.setColor(palette().color(QPalette::Active, QPalette::Base));
        brush.setStyle(Qt::SolidPattern);
    }
    painter->fillRect(rect, brush);

    if (!current.intersected(m_window).isEmpty()){
        brush.setColor(QColor(0, 127, 255));
        brush.setStyle(Qt::FDiagPattern);
        painter->fillRect(rect, brush);
    }

    if (m_syncedTimeStorage.periods(Qn::MotionRole).intersects(current)){
        brush.setColor(QColor(255, 0, 0));
        brush.setStyle(Qt::BDiagPattern);
    }
    else if (m_syncedTimeStorage.periods(Qn::RecordingRole).intersects(current)){
        brush.setColor(QColor(0, 220, 0));
        brush.setStyle(Qt::BDiagPattern);
    }
    painter->fillRect(rect, brush);

    QnScopedPainterPenRollback penRollback(painter);
    Q_UNUSED(penRollback);

    QnScopedPainterFontRollback fontRollback(painter);
    Q_UNUSED(fontRollback);

    QPen pen = painter->pen();
    pen.setColor(Qt::black);
    pen.setWidth(1);
    painter->setPen(pen);
    painter->drawRect(rect);

    QFont font = painter->font();
    font.setPixelSize(12);

    QString text = QString::number(date.day());
    if (date < this->minimumDate() || date > this->maximumDate()){
        pen.setColor(palette().color(QPalette::Disabled, QPalette::Text));
    }
    else{
    //    pen.setColor(date.dayOfWeek() > 5 ? Qt::red : Qt::white);
        pen.setColor(palette().color(QPalette::Active, QPalette::Text));
        font.setBold(true);
    }

    painter->setPen(pen);
    painter->setFont(font);
    painter->drawText(rect, Qt::AlignCenter, text);
}

void QnCalendarWidget::dateChanged(const QDate &date){
    qDebug() << "date changed" << date;
    emit dateUpdate(date);
}
