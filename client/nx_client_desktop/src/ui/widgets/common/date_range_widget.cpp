#include "date_range_widget.h"
#include "ui_date_range_widget.h"

#include <QtCore/QDateTime>

#include <common/common_module.h>

#include <client/client_settings.h>

#include <core/resource/media_server_resource.h>

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
    ui(new Ui::DateRangeWidget())
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
    const QDate start = displayDate(startTimeMs);
    const QDate end = displayDate(endTimeMs);

    if (start == startDate() && end == endDate())
        return;

    QSignalBlocker blockFrom(ui->dateEditFrom);
    ui->dateEditFrom->setDateRange(minAllowedDate(), maxAllowedDate());
    ui->dateEditFrom->setDate(start);

    QSignalBlocker blockTo(ui->dateEditTo);
    ui->dateEditTo->setDateRange(minAllowedDate(), maxAllowedDate());
    ui->dateEditTo->setDate(end);

    updateRange();
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
    updateAllowedRange();
    emit rangeChanged(startTimeMs(), endTimeMs());
}

void QnDateRangeWidget::updateAllowedRange()
{
    ui->dateEditFrom->setDateRange(minAllowedDate(), ui->dateEditTo->date());
    ui->dateEditTo->setDateRange(ui->dateEditFrom->date(), maxAllowedDate());
}

QDateTime QnDateRangeWidget::actualDateTime(const QDate& userDate) const
{
    // QDateTime is created from date, thus it always started from the start of the day in the
    // current timezone
    if (qnSettings->timeMode() == Qn::ClientTimeMode)
        return QDateTime(userDate);

    const auto server = commonModule()->currentServer();
    const auto serverUtcOffsetMs = server->utcOffset();

    static const QTime kMidnight(0, 0);
    return (serverUtcOffsetMs != Qn::InvalidUtcOffset)
        ? QDateTime(userDate, kMidnight, Qt::OffsetFromUTC, serverUtcOffsetMs / kMillisecondsInSeconds)
        : QDateTime(userDate, kMidnight, Qt::UTC);
}

QDate QnDateRangeWidget::displayDate(qint64 timestampMs) const
{
    return context()->instance<QnWorkbenchServerTimeWatcher>()->displayTime(timestampMs).date();
}
