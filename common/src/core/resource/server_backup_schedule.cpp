#include "server_backup_schedule.h"

//TODO: #GDM dependency will be removed together with this class itself
#include <nx_ec/data/api_media_server_data.h>

#include <utils/common/model_functions.h>

namespace
{
    enum 
    {
        bitsPerMegabit = 1000000
        , bitsPerByte = 8
    };
    
    const int kDefauNotCappedBitrateMBits = -100 * bitsPerMegabit / bitsPerByte;
}

const int QnServerBackupSchedule::defaultBackupBitrate = kDefauNotCappedBitrateMBits;

QnServerBackupSchedule::QnServerBackupSchedule() 
    : backupType(Qn::Backup_Manual)
    , backupDaysOfTheWeek(ec2::backup::AllDays) // all days of week
    , backupStartSec(0) // midnight
    , backupDurationSec(-1) // unlimited duration
    , backupBitrate(kDefauNotCappedBitrateMBits) // unlimited
{}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnServerBackupSchedule), (eq), _Fields)


