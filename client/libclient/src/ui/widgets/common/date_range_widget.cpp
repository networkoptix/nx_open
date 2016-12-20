#include "date_range_widget.h"
#include "ui_date_range_widget.h"

#include <QtCore/QDateTime>

#include <common/common_module.h>

#include <client/client_settings.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

namespace {

constexpr int kMillisecondsInSeconds = 1000;
constexpr int kMillisecondsInDay = 60 * 60 * 24 * 1000;
constexpr int kDefaultDaysOffset = -7;


QDate defaultStartDate()
{
    return QDate::currentDate().addDays(kDefaultDaysOffset);
}

qint64 getStartOfTheDayMs(qint64 timeMs)
{
    QDateTime value(QDateTime::fromMSecsSinceEpoch(timeMs));
    value.setTime(QTime(0, 0));
    return value.toMSecsSinceEpoch();
}

qint64 getEndOfTheDayMs(qint64 timeMs)
{
    QDateTime value(QDateTime::fromMSecsSinceEpoch(timeMs));
    value.setTime(QTime(0, 0));
    value.addDays(1);
    return value.toMSecsSinceEpoch();
}

QDate minAllowedDate()
{
    static const QDate kMinAllowedDate(2000, 1, 1);
    return kMinAllowedDate;
}

QDate maxAllowedDate()
{
    // 1 month forward should cover all local timezones diffs.
    return QDate::currentDate().addMonths(1);
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
    ui->dateEditFrom->setDateRange(minAllowedDate(), maxAllowedDate());
    ui->dateEditFrom->setDate(displayDate(start));

    QSignalBlocker blockTo(ui->dateEditTo);
    ui->dateEditTo->setDateRange(minAllowedDate(), maxAllowedDate());
    ui->dateEditTo->setDate(displayDate(end));

    updateAllowedRange();
    emit rangeChanged(start, end);
}

QDate QnDateRangeWidget::startDate() const
{
    return ui->dateEditFrom->date();
}

QDate QnDateRangeWidget::endDate() const
{
    return ui->dateEditTo->date();
}

void QnDateRangeWidget::reset()
{
    setRange(QDateTime(defaultStartDate()).toMSecsSinceEpoch(), QDateTime::currentMSecsSinceEpoch());
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
    ui->dateEditFrom->setDateRange(minAllowedDate(), ui->dateEditTo->date());
    ui->dateEditTo->setDateRange(ui->dateEditFrom->date(), maxAllowedDate());
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
