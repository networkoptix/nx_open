#pragma once

#include <QtCore/QDateTime>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/types_fwd.h>
#include <nx/vms/api/types/days_of_week.h>

/** Backup storage settings */
struct QnServerBackupSchedule
{
    nx::vms::api::BackupType backupType;
    nx::vms::api::DaysOfWeek backupDaysOfTheWeek; // Days of the week mask. -1 if not set
    int     backupStartSec;      // seconds from 00:00:00. Error if rDOW set and this is not set
    int     backupDurationSec;   // duration of synchronization period in seconds. -1 if not set.
    int     backupBitrate;       // bitrate cap in bytes per second. Any value <= 0 if not capped.

    static const int defaultBackupBitrate;
    static constexpr int kDurationUntilFinished = -1;
    QnServerBackupSchedule();

    bool isValid() const;
    bool isUntilFinished() const;

    /**
     * Checks if dateTime is within any period of the schedule and within the day
     * of the period if isUntilFinished() is true.
     */
    bool dateTimeFits(QDateTime dateTime) const;

    /** Checks if dateTime day fits and dateTime is before period end. */
    bool isDateTimeBeforeDayPeriodEnd(QDateTime dateTime) const;
};

#define QnServerBackupSchedule_Fields  \
    (backupType)                     \
    (backupDaysOfTheWeek)            \
    (backupStartSec)                 \
    (backupDurationSec)              \
    (backupBitrate)                  \

QN_FUSION_DECLARE_FUNCTIONS(QnServerBackupSchedule, (eq)(metatype))
