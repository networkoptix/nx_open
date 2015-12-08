#pragma once

#include <common/common_globals.h>

#include <utils/common/model_functions_fwd.h>

/** Backup storage settings */
struct QnServerBackupSchedule {
    Qn::BackupType backupType;
    int     backupDaysOfTheWeek; // Days of the week mask. -1 if not set
    int     backupStartSec;      // seconds from 00:00:00. Error if rDOW set and this is not set
    int     backupDurationSec;   // duration of synchronization period in seconds. -1 if not set.
    int     backupBitrate;       // bitrate cap in bytes per second. Any value <= 0 if not capped.

    static const int defaultBackupBitrate;
    QnServerBackupSchedule();

    bool isValid() const;
};

#define QnServerBackupSchedule_Fields  \
    (backupType)                     \
    (backupDaysOfTheWeek)            \
    (backupStartSec)                 \
    (backupDurationSec)              \
    (backupBitrate)                  \


QN_FUSION_DECLARE_FUNCTIONS(QnServerBackupSchedule, (eq)(metatype))
