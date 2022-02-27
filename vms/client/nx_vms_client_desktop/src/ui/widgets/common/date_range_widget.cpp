// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "date_range_widget.h"
#include "ui_date_range_widget.h"

#include <QtCore/QDateTime>

#include <common/common_module.h>

#include <nx/vms/time/formatter.h>

#include <client/client_settings.h>
#include <nx/vms/client/desktop/common/utils/current_date_monitor.h>

#include <core/resource/media_server_resource.h>

#include <ui/workbench/workbench_context.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>

namespace {

constexpr int kMillisecondsInSeconds = 1000;
constexpr int kMillisecondsInDay = 60 * 60 * 24 * 1000;
constexpr int kDefaultDaysOffset = -7;

using CurrentDateMonitor = nx::vms::client::desktop::CurrentDateMonitor;

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
    // 1 day forward should cover all local timezones diffs.
    return QDate::currentDate().addDays(1);
}

} // namespace

QnDateRangeWidget::QnDateRangeWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::DateRangeWidget())
{
    ui->setupUi(this);

    ui->dateEditFrom->setDisplayFormat(getFormatString(nx::vms::time::Format::dd_MM_yyyy));
    ui->dateEditTo->setDisplayFormat(getFormatString(nx::vms::time::Format::dd_MM_yyyy));

    reset();

    connect(ui->dateEditFrom, &QDateEdit::userDateChanged, this,
        &QnDateRangeWidget::updateRange);
    connect(ui->dateEditTo, &QDateEdit::userDateChanged, this,
        &QnDateRangeWidget::updateRange);

    auto currentDateMonitor = new CurrentDateMonitor(this);
    connect(currentDateMonitor, &CurrentDateMonitor::currentDateChanged,
        this, &QnDateRangeWidget::updateAllowedRange);
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
    const auto serverUtcOffsetMs = server
        ? server->utcOffset()
        : Qn::InvalidUtcOffset;

    static const QTime kMidnight(0, 0);
    return (serverUtcOffsetMs != Qn::InvalidUtcOffset)
        ? QDateTime(userDate, kMidnight, Qt::OffsetFromUTC, serverUtcOffsetMs / kMillisecondsInSeconds)
        : QDateTime(userDate, kMidnight, Qt::UTC);
}

QDate QnDateRangeWidget::displayDate(qint64 timestampMs) const
{
    const auto timeWatcher = context()->instance<nx::vms::client::core::ServerTimeWatcher>();
    return timeWatcher->displayTime(timestampMs).date();
}
