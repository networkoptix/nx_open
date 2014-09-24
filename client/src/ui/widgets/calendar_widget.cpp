#include "calendar_widget.h"

#include <QtGui/QPainter>
#include <QtWidgets/QTableView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QToolButton>
#include <QtGui/QWheelEvent>
#include <QtGui/QMouseEvent>

#include <recording/time_period_list.h>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/synctime.h>
#include <utils/math/color_transformations.h>

#include <ui/style/globals.h>
#include <ui/delegates/calendar_item_delegate.h>
#include <ui/common/palette.h>

namespace {
    enum {
        DAY = 1000 * 60 * 60 * 24
    };

} // anonymous namespace


QnCalendarWidget::QnCalendarWidget(QWidget *parent):
    base_type(parent),
    m_delegate(new QnCalendarItemDelegate(this)),
    m_empty(true),
    m_currentTimePeriodsVisible(false),
    m_currentTime(0),
    m_localOffset(0)
{
    setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);

    /* Month button's drop-down menu doesn't work well with graphics scene, so we simply remove it. */
    QToolButton *monthButton = findChild<QToolButton *>(QLatin1String("qt_calendar_monthbutton"));
    monthButton->setMenu(NULL);

    QWidget *yearEdit = findChild<QWidget *>(QLatin1String("qt_calendar_yearedit"));
    
    QnSingleEventEater *eater = new QnSingleEventEater(Qn::AcceptEvent, this);
    eater->setEventType(QEvent::ContextMenu);
    yearEdit->installEventFilter(eater);

    QTableView *tableView = findChild<QTableView *>(QLatin1String("qt_calendar_calendarview"));
    tableView->horizontalHeader()->setMinimumSectionSize(18);
    connect(tableView, SIGNAL(changeDate(const QDate &, bool)), this, SIGNAL(dateClicked(const QDate &)));
    //setPaletteColor(tableView, QPalette::Highlight, QColor(0, 0, 0, 255));

    QnSingleEventSignalizer *signalizer = new QnSingleEventSignalizer(this);
    signalizer->setEventType(QEvent::Paint);
    tableView->viewport()->installEventFilter(signalizer);
    connect(signalizer, SIGNAL(activated(QObject *, QEvent *)), this, SLOT(updateCurrentTime()));

    QAbstractItemModel *model = tableView->model();
    connect(model, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(updateEnabledPeriod()));

    QWidget *navigationBar = findChild<QWidget *>(QLatin1String("qt_calendar_navigationbar"));
    navigationBar->setBackgroundRole(QPalette::Window);

    updateEnabledPeriod();
    updateCurrentTime();
    updateEmpty();
}

void QnCalendarWidget::setCurrentTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList &periods)
{
    m_currentPeriodStorage.setPeriods(type, periods);
    updateEmpty();
    update();
}

void QnCalendarWidget::setSyncedTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList &periods) {
    m_syncedPeriodStorage.setPeriods(type, periods);
    updateEmpty();
    update();
}

void QnCalendarWidget::setSelectedWindow(quint64 windowStart, quint64 windowEnd) {
    m_selectedPeriod = QnTimePeriod(windowStart, windowEnd - windowStart);

    if(m_selectedPeriod.startTimeMs >= m_selectedDaysPeriod.startTimeMs && m_selectedPeriod.startTimeMs < m_selectedDaysPeriod.startTimeMs + DAY &&
        m_selectedPeriod.endTimeMs() <= m_selectedDaysPeriod.endTimeMs() && m_selectedPeriod.endTimeMs() > m_selectedDaysPeriod.endTimeMs() - DAY)
        return; /* Ok, no update needed. */

    qint64 dayWindowStart = QDateTime(QDateTime::fromMSecsSinceEpoch(windowStart).date(), QTime()).toMSecsSinceEpoch();
    qint64 dayWindowEnd = QDateTime(QDateTime::fromMSecsSinceEpoch(windowEnd + DAY - 1).date(), QTime()).toMSecsSinceEpoch();
    QnTimePeriod dayWindow = QnTimePeriod(dayWindowStart - m_localOffset, dayWindowEnd - dayWindowStart);
    if(m_selectedDaysPeriod == dayWindow)
        return;

    m_selectedDaysPeriod = dayWindow;
    update();
}

bool QnCalendarWidget::currentTimePeriodsVisible() const {
    return m_currentTimePeriodsVisible;
}

void QnCalendarWidget::setCurrentTimePeriodsVisible(bool value){
    if (m_currentTimePeriodsVisible == value)
        return;

    m_currentTimePeriodsVisible = value;
    update();
}

bool QnCalendarWidget::isEmpty() {
    return m_empty;
}

void QnCalendarWidget::paintCell(QPainter *painter, const QRect &rect, const QDate &date) const {
    QnTimePeriod period(QDateTime(date).toMSecsSinceEpoch(), DAY); 
    if (period.startTimeMs > m_currentTime)
        period = QnTimePeriod();

    m_delegate->paintCell(
        painter, 
        palette(), 
        rect, 
        period, 
        m_localOffset,
        m_enabledPeriod, 
        m_selectedPeriod, 
        m_currentTimePeriodsVisible ? m_currentPeriodStorage : m_emptyPeriodStorage,
        m_syncedPeriodStorage, 
        QString::number(date.day())
    );
}

void QnCalendarWidget::updateEmpty() {
    bool value = true;
    for(int type = 0; type < Qn::TimePeriodContentCount; type++) {
        if (!m_currentPeriodStorage.periods(static_cast<Qn::TimePeriodContent>(type)).empty() ||
            !m_syncedPeriodStorage.periods(static_cast<Qn::TimePeriodContent>(type)).empty())
        {
            value = false;
            break;
        }
    }
    if (m_empty == value)
        return;

    m_empty = value;
    emit emptyChanged();
}

void QnCalendarWidget::updateEnabledPeriod() {
    qint64 startMSecs = QDateTime(minimumDate()).toMSecsSinceEpoch();
    qint64 endMSecs = QDateTime(maximumDate()).addDays(1).toMSecsSinceEpoch();

    m_enabledPeriod = QnTimePeriod(startMSecs, endMSecs - startMSecs);
}

void QnCalendarWidget::updateCurrentTime() {
    m_currentTime = qnSyncTime->currentMSecsSinceEpoch();
}

void QnCalendarWidget::wheelEvent(QWheelEvent *event) {
    event->accept(); /* Do not propagate wheel events past the calendar widget. */
}

void QnCalendarWidget::mousePressEvent(QMouseEvent *event) {
    event->accept(); /* Prevent surprising click-through scenarios. */
}

void QnCalendarWidget::mouseReleaseEvent(QMouseEvent *event) {
    event->accept(); /* Prevent surprising click-through scenarios. */
}

qint64 QnCalendarWidget::localOffset() const {
    return m_localOffset;
}

void QnCalendarWidget::setLocalOffset(qint64 localOffset) {
    if(m_localOffset == localOffset)
        return;

    m_localOffset = localOffset;

    update();
}
