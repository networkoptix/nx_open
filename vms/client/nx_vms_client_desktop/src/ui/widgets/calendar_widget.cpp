#include "calendar_widget.h"

#include <QtGui/QPainter>
#include <QtWidgets/QTableView>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QToolButton>
#include <QtGui/QWheelEvent>
#include <QtGui/QMouseEvent>

#include <recording/time_period_list.h>

#include <nx/utils/log/log.h>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/synctime.h>
#include <utils/math/color_transformations.h>

#include <ui/style/globals.h>
#include <ui/style/helper.h>
#include <ui/delegates/calendar_item_delegate.h>
#include <ui/common/palette.h>

namespace {
    enum {
        DAY = 1000 * 60 * 60 * 24
    };

} // anonymous namespace


QnCalendarWidget::QnCalendarWidget(QWidget *parent)
    : base_type(parent)
    , m_delegate(new QnCalendarItemDelegate(this))
    , m_empty(true)
    , m_currentTimePeriodsVisible(false)
    , m_currentTime(0)
    , m_localOffset(0)
    , m_navigationBar(nullptr)
{
    setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);

    setProperty(style::Properties::kDontPolishFontProperty, true);

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

    installEventHandler(tableView->viewport(), QEvent::Paint, this, &QnCalendarWidget::updateCurrentTime);

    tableView->ensurePolished();
    setPaletteColor(tableView, QPalette::AlternateBase, tableView->palette().color(QPalette::Base));

    QAbstractItemModel *model = tableView->model();
    connect(model, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(updateEnabledPeriod()));

    m_navigationBar = findChild<QWidget *>(QLatin1String("qt_calendar_navigationbar"));

    NX_ASSERT(m_navigationBar, "Navigator bar is nullptr!");
    m_navigationBar->setBackgroundRole(QPalette::Window);

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

void QnCalendarWidget::setSelectedWindow(quint64 windowStart, quint64 windowEnd)
{
    QnTimePeriod period = QnTimePeriod(windowStart + m_localOffset, windowEnd - windowStart);

    // Calculate the first day of the window.
    QDateTime dayStart = QDateTime::fromMSecsSinceEpoch(period.startTimeMs);
    qint64 dayWindowStartMs = QDateTime(dayStart.date(), QTime()).toMSecsSinceEpoch();
    // Calculate the last day of the window.

    QDateTime dayEnd = QDateTime::fromMSecsSinceEpoch(period.endTimeMs() + DAY - 1);
    qint64 dayWindowEndMs = QDateTime(dayEnd.date(), QTime()).toMSecsSinceEpoch();

    QnTimePeriod dayWindow(dayWindowStartMs, dayWindowEndMs - dayWindowStartMs - 1);

    if (m_selectedPeriod == dayWindow)
        return;

    m_selectedPeriod = dayWindow;

    NX_VERBOSE(this) << "QnCalendarWidget::setSelectedWindow selectedPeriod = " << m_selectedPeriod << " from period = " << period;
    NX_VERBOSE(this) << "QnCalendarWidget::setSelectedWindow local time ="
        << QDateTime::fromMSecsSinceEpoch(this->m_currentTime)
        << " server time =" << QDateTime::fromMSecsSinceEpoch(this->m_currentTime - m_localOffset);
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

void QnCalendarWidget::paintCell(QPainter *painter, const QRect &rect, const QDate &date) const
{
    QnTimePeriod localPeriod(QDateTime(date).toMSecsSinceEpoch() - m_localOffset, DAY);
    if (localPeriod.startTimeMs > m_currentTime)
        localPeriod.startTimeMs = QnTimePeriod().startTimeMs;

    const bool isEnabled = m_enabledPeriod.intersects(localPeriod);
    const bool isSelected = m_selectedPeriod.intersects(localPeriod);

    QDateTime day(QDateTime::fromMSecsSinceEpoch(localPeriod.startTimeMs).date(), QTime());

    const int dayOfWeek = day.date().dayOfWeek();
    const auto foregroundRole = (dayOfWeek == Qt::Saturday || dayOfWeek == Qt::Sunday) && isEnabled
        ? QPalette::BrightText
        : QPalette::Text;

    const QnTimePeriodStorage& primaryPeriods = (m_currentTimePeriodsVisible
        ? m_currentPeriodStorage
        : m_emptyPeriodStorage);

    m_delegate->paintCell(painter, rect, localPeriod, primaryPeriods, m_syncedPeriodStorage, isSelected);
    m_delegate->paintCellText(painter, palette(), rect,
        QString::number(date.day()), isEnabled, foregroundRole);
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

void QnCalendarWidget::updateEnabledPeriod()
{
    qint64 enabledStartMs = QDateTime(minimumDate()).toMSecsSinceEpoch();
    qint64 enabledEndMs = QDateTime(maximumDate()).addDays(1).toMSecsSinceEpoch();

    m_enabledPeriod = QnTimePeriod(enabledStartMs + m_localOffset, enabledEndMs - enabledStartMs);
    NX_VERBOSE(this) << "QnCalendarWidget::updateEnabledPeriod() enabledPeriod = " << m_enabledPeriod;
    return;
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

void QnCalendarWidget::setLocalOffset(qint64 localOffset)
{
    if(m_localOffset == localOffset)
        return;

    m_localOffset = localOffset;

    updateEnabledPeriod();
    update();
}

int QnCalendarWidget::headerHeight() const
{
    return m_navigationBar->height();
}

