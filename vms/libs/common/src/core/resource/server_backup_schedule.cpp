#include "server_backup_schedule.h"

// TODO: #GDM dependency will be removed together with this class itself
#include <nx/vms/api/data/media_server_data.h>

#include <nx/fusion/model_functions.h>

const int QnServerBackupSchedule::defaultBackupBitrate
    = nx::vms::api::MediaServerUserAttributesData::kDefaultBackupBitrate;

QnServerBackupSchedule::QnServerBackupSchedule():
    backupType(nx::vms::api::BackupType::manual),
    backupDaysOfTheWeek(nx::vms::api::DayOfWeek::all), // all days of week
    backupStartSec(0), // midnight
    backupDurationSec(-1), // unlimited duration
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnServerBackupSchedule), (eq), _Fields)
