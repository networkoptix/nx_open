#include "date_range_widget.h"
#include "ui_date_range_widget.h"

#include <QtCore/QDateTime>

#include <common/common_module.h>

#include <client/client_settings.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

namespace {

static const int kMillisecondsInSeconds = 1000;
static const int kMillisecondsInDay = 60 * 60 * 24 * 1000;

static const QDateTime kDefaultStartDate(QDate(2000, 1, 1));

qint64 getStartOfTheDayMs(qint64 timeMs)
{
    return timeMs - (timeMs % kMillisecondsInDay);
}

qint64 getEndOfTheDayMs(qint64 timeMs)
{
    return getStartOfTheDayMs(timeMs) + kMillisecondsInDay;
}

QDateTime maxAllowedDate()
{
    // 1 month forward should cover all local timezones diffs.
    return QDateTime::currentDateTime().addMonths(1);
}

}

QnDateRangeWidget::QnDateRangeWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::DateRangeWidget()),
    m_startTimeMs(0),
    m_endTimeMs(0)
{
    ui->setupUi(this);

    reset();

    connect(ui->dateEditFrom, &QDateEdit::userDateChanged, this,
        &QnDateRangeWidget::updateRange);
    connect(ui->dateEditTo, &QDateEdit::userDateChanged, this,
        &QnDateRangeWidget::updateRange);
}

QnDateRangeWidget::~QnDateRangeWidget()
{
}

qint64 QnDateRangeWidget::startTimeMs() const
{
    return actualDateTime(ui->dateEditFrom->date()).toMSecsSinceEpoch();
}

qint64 QnDateRangeWidget::endTimeMs() const
{
    return actualDateTime(ui->dateEditTo->date()).toMSecsSinceEpoch() + kMillisecondsInDay;
}

void QnDateRangeWidget::setRange(qint64 startTimeMs, qint64 endTimeMs)
{
    /* Working with the 1-day precision */
    qint64 start = getStartOfTheDayMs(startTimeMs);
    qint64 end = getEndOfTheDayMs(endTimeMs);

    if (m_startTimeMs == start && m_endTimeMs == end)
        return;

    m_startTimeMs = start;
    m_endTimeMs = end;

    QSignalBlocker blockFrom(ui->dateEditFrom);
    ui->dateEditFrom->setDateRange(kDefaultStartDate.date(), maxAllowedDate().date());
    ui->dateEditFrom->setDate(displayDate(start));

    QSignalBlocker blockTo(ui->dateEditTo);
    ui->dateEditTo->setDateRange(kDefaultStartDate.date(), maxAllowedDate().date());
    ui->dateEditTo->setDate(displayDate(end));

    updateAllowedRange();
    emit rangeChanged(start, end);
}

void QnDateRangeWidget::reset()
{
    setRange(kDefaultStartDate.toMSecsSinceEpoch(), QDateTime::currentMSecsSinceEpoch());
}

void QnDateRangeWidget::updateRange()
{
    auto start = startTimeMs();
    auto end = endTimeMs();
    if (m_startTimeMs == start && m_endTimeMs == end)
        return;

    updateAllowedRange();

    m_startTimeMs = start;
    m_endTimeMs = end;
    emit rangeChanged(start, end);
}

void QnDateRangeWidget::updateAllowedRange()
{
    ui->dateEditFrom->setDateRange(kDefaultStartDate.date(), ui->dateEditTo->date());
    ui->dateEditTo->setDateRange(ui->dateEditFrom->date(), maxAllowedDate().date());
}

QDateTime QnDateRangeWidget::actualDateTime(const QDate &userDate) const
{
    // QDateTime is created from date, thus it always started from the start of the day in the
    // current timezone
    if (qnSettings->timeMode() == Qn::ClientTimeMode)
        return QDateTime(userDate);

    const auto timeWatcher = context()->instance<QnWorkbenchServerTimeWatcher>();
    const auto server = qnCommon->currentServer();
    const auto serverUtcOffsetSecs = timeWatcher->utcOffset(server) / kMillisecondsInSeconds;
    return QDateTime(userDate, QTime(0, 0), Qt::OffsetFromUTC, serverUtcOffsetSecs);
}

QDate QnDateRangeWidget::displayDate(qint64 timestampMs) const
{
    return context()->instance<QnWorkbenchServerTimeWatcher>()->displayTime(timestampMs).date();
}
