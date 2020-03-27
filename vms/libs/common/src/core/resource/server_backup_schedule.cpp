#include "server_backup_schedule.h"

// TODO: #GDM dependency will be removed together with this class itself
#include <nx/vms/api/data/media_server_data.h>

#include <nx/fusion/model_functions.h>

using namespace std::chrono;

const int QnServerBackupSchedule::defaultBackupBitrate
    = nx::vms::api::MediaServerUserAttributesData::kDefaultBackupBitrate;

QnServerBackupSchedule::QnServerBackupSchedule():
    backupType(nx::vms::api::BackupType::manual),
    backupDaysOfTheWeek(nx::vms::api::DayOfWeek::all), // all days of week
    backupStartSec(0), // midnight
    backupDurationSec(kDurationUntilFinished),
    backupBitrate(defaultBackupBitrate) // unlimited
{
}

bool QnServerBackupSchedule::isValid() const
{
    /* Realtime and manual backup do not take schedule into account. */
    if (backupType != nx::vms::api::BackupType::scheduled)
        return true;

    /* Check if no days are selected. */
    return backupDaysOfTheWeek != 0;
}


bool QnServerBackupSchedule::isUntilFinished() const
{
    return backupDurationSec == kDurationUntilFinished;
}

bool QnServerBackupSchedule::dateTimeFits(QDateTime dateTime) const
{
    if (!isDateTimeBeforeDayPeriodEnd(dateTime))
        return false;

    return milliseconds(dateTime.time().msecsSinceStartOfDay()) > seconds(backupStartSec);
}

bool QnServerBackupSchedule::isDateTimeBeforeDayPeriodEnd(QDateTime dateTime) const
{
    const auto today = nx::vms::api::dayOfWeek(Qt::DayOfWeek(dateTime.date().dayOfWeek()));
    if (!backupDaysOfTheWeek.testFlag(today))
        return false;
    if (isUntilFinished())
        return true;

    const milliseconds periodEnd = seconds(backupStartSec + backupDurationSec);
    return milliseconds(dateTime.time().msecsSinceStartOfDay()) < periodEnd;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnServerBackupSchedule), (eq), _Fields)
