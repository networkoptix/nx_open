#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QLocale>
#include <QtCore/QTime>

#include "core/resource/server_backup_schedule.h"
#include "nx/vms/api/types/days_of_week.h"
#include "nx/vms/api/types/resource_types.h"

using namespace nx::vms;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace nx::core::resource::test {

static const QDate someMonday(2019, 12, 30);
static const QDate someTuesday = someMonday.addDays(1);
static const QDate someSaturday = someMonday.addDays(5);
static const QDate someSunday = someMonday.addDays(6);

enum EndTime
{
    EndAt18,
    EndAt3NextDay,
    UntilFinished,
};

static QnServerBackupSchedule getSchedule(EndTime endTime)
{
    QnServerBackupSchedule schedule;
    schedule.backupType = api::BackupType::scheduled;
    schedule.backupDaysOfTheWeek = api::DayOfWeek::monday | api::DayOfWeek::sunday;
    schedule.backupStartSec = seconds(7h + 0min + 0s).count();
    switch (endTime)
    {
        case EndAt18:
            schedule.backupDurationSec = seconds(11h + 0min + 0s).count();
            break;
        case EndAt3NextDay:
            schedule.backupDurationSec = seconds(20h + 0min + 0s).count();
            break;
        case UntilFinished:
            schedule.backupDurationSec = QnServerBackupSchedule::kDurationUntilFinished;
    }
    return schedule;
}

static const std::vector<QnServerBackupSchedule> schedules = {
    getSchedule(EndAt18),
    getSchedule(EndAt3NextDay),
    getSchedule(UntilFinished),
};

static const std::vector<std::string> endTimeStrings = {
    "ends_at_18_00",
    "ends_at_3_00_on_next_day",
    "lasts_until_finished",
};

struct DateTimeTestCase
{
    EndTime endTime;
    QDate date;
    QTime time;
    bool beforeDayPeriodEndResult;
    bool dateTimeFitsResult;

    QDateTime dateTime() const { return QDateTime(date, time); }
};

static const std::vector<DateTimeTestCase> kDateTimeTestCases = {
    { EndAt18, someMonday, {0, 0, 0}, true, false },
    { EndAt18, someMonday, {6, 59, 59}, true, false },
    { EndAt18, someMonday, {7, 0, 0}, true, false },
    { EndAt18, someMonday, {7, 0, 1}, true, true },
    { EndAt18, someMonday, {17, 59, 59}, true, true },
    { EndAt18, someMonday, {18, 0, 0}, false, false },
    { EndAt18, someMonday, {18, 0, 1}, false, false },
    { EndAt18, someMonday, {23, 59, 59}, false, false },

    { EndAt18, someTuesday, {0, 0, 0}, false, false },
    { EndAt18, someTuesday, {6, 59, 59}, false, false },
    { EndAt18, someTuesday, {7, 0, 0}, false, false },
    { EndAt18, someTuesday, {7, 0, 1}, false, false },
    { EndAt18, someTuesday, {17, 59, 59}, false, false },
    { EndAt18, someTuesday, {18, 0, 0}, false, false },
    { EndAt18, someTuesday, {18, 0, 1}, false, false },
    { EndAt18, someTuesday, {23, 59, 59}, false, false },

    { EndAt18, someSaturday, {0, 0, 0}, false, false },
    { EndAt18, someSaturday, {6, 59, 59}, false, false },
    { EndAt18, someSaturday, {7, 0, 0}, false, false },
    { EndAt18, someSaturday, {7, 0, 1}, false, false },
    { EndAt18, someSaturday, {17, 59, 59}, false, false },
    { EndAt18, someSaturday, {18, 0, 0}, false, false },
    { EndAt18, someSaturday, {18, 0, 1}, false, false },
    { EndAt18, someSaturday, {23, 59, 59}, false, false },

    { EndAt18, someSunday, {0, 0, 0}, true, false },
    { EndAt18, someSunday, {6, 59, 59}, true, false },
    { EndAt18, someSunday, {7, 0, 0}, true, false },
    { EndAt18, someSunday, {7, 0, 1}, true, true },
    { EndAt18, someSunday, {17, 59, 59}, true, true },
    { EndAt18, someSunday, {18, 0, 0}, false, false },
    { EndAt18, someSunday, {18, 0, 1}, false, false },
    { EndAt18, someSunday, {23, 59, 59}, false, false },

    { EndAt3NextDay, someMonday, {0, 0, 0}, true, false },
    { EndAt3NextDay, someMonday, {2, 59, 59}, true, false },
    { EndAt3NextDay, someMonday, {3, 0, 0}, true, false },
    { EndAt3NextDay, someMonday, {3, 0, 1}, true, false },
    { EndAt3NextDay, someMonday, {6, 59, 59}, true, false },
    { EndAt3NextDay, someMonday, {7, 0, 0}, true, false },
    { EndAt3NextDay, someMonday, {7, 0, 1}, true, true },
    { EndAt3NextDay, someMonday, {23, 59, 59}, true, true },

    { EndAt3NextDay, someTuesday, {0, 0, 0}, false, false },
    { EndAt3NextDay, someTuesday, {2, 59, 59}, false, false },
    { EndAt3NextDay, someTuesday, {3, 0, 0}, false, false },
    { EndAt3NextDay, someTuesday, {3, 0, 1}, false, false },
    { EndAt3NextDay, someTuesday, {6, 59, 59}, false, false },
    { EndAt3NextDay, someTuesday, {7, 0, 0}, false, false },
    { EndAt3NextDay, someTuesday, {7, 0, 1}, false, false },
    { EndAt3NextDay, someTuesday, {23, 59, 59}, false, false },

    { EndAt3NextDay, someSaturday, {0, 0, 0}, false, false },
    { EndAt3NextDay, someSaturday, {2, 59, 59}, false, false },
    { EndAt3NextDay, someSaturday, {3, 0, 0}, false, false },
    { EndAt3NextDay, someSaturday, {3, 0, 1}, false, false },
    { EndAt3NextDay, someSaturday, {6, 59, 59}, false, false },
    { EndAt3NextDay, someSaturday, {7, 0, 0}, false, false },
    { EndAt3NextDay, someSaturday, {7, 0, 1}, false, false },
    { EndAt3NextDay, someSaturday, {23, 59, 59}, false, false },

    { EndAt3NextDay, someSunday, {0, 0, 0}, true, false },
    { EndAt3NextDay, someSunday, {2, 59, 59}, true, false },
    { EndAt3NextDay, someSunday, {3, 0, 0}, true, false },
    { EndAt3NextDay, someSunday, {3, 0, 1}, true, false },
    { EndAt3NextDay, someSunday, {6, 59, 59}, true, false },
    { EndAt3NextDay, someSunday, {7, 0, 0}, true, false },
    { EndAt3NextDay, someSunday, {7, 0, 1}, true, true },
    { EndAt3NextDay, someSunday, {23, 59, 59}, true, true },

    { UntilFinished, someMonday, {0, 0, 0}, true, false },
    { UntilFinished, someMonday, {6, 59, 59}, true, false },
    { UntilFinished, someMonday, {7, 0, 0}, true, false },
    { UntilFinished, someMonday, {7, 0, 1}, true, true },
    { UntilFinished, someMonday, {23, 59, 59}, true, true },

    { UntilFinished, someTuesday, {0, 0, 0}, false, false },
    { UntilFinished, someTuesday, {6, 59, 59}, false, false },
    { UntilFinished, someTuesday, {7, 0, 0}, false, false },
    { UntilFinished, someTuesday, {7, 0, 1}, false, false },
    { UntilFinished, someTuesday, {23, 59, 59}, false, false },

    { UntilFinished, someSaturday, {0, 0, 0}, false, false },
    { UntilFinished, someSaturday, {6, 59, 59}, false, false },
    { UntilFinished, someSaturday, {7, 0, 0}, false, false },
    { UntilFinished, someSaturday, {7, 0, 1}, false, false },
    { UntilFinished, someSaturday, {23, 59, 59}, false, false },

    { UntilFinished, someSunday, {0, 0, 0}, true, false },
    { UntilFinished, someSunday, {6, 59, 59}, true, false },
    { UntilFinished, someSunday, {7, 0, 0}, true, false },
    { UntilFinished, someSunday, {7, 0, 1}, true, true },
    { UntilFinished, someSunday, {23, 59, 59}, true, true },
};

std::string dateTimeTestCaseName(::testing::TestParamInfo<DateTimeTestCase> testCaseInfo)
{
    const DateTimeTestCase& testCase = testCaseInfo.param;
    return QLocale(QLocale::English).toString(testCase.dateTime(), "ddd_hh_mm_ss").toStdString()
        + "__backup_" + endTimeStrings[testCase.endTime];
}

class DateTimeMethodsTest : public ::testing::TestWithParam<DateTimeTestCase>
{
};

TEST_P(DateTimeMethodsTest, checkIsDateTimeBeforeDayPeriodEnd)
{
    const DateTimeTestCase& testCase = GetParam();
    const bool actualResult =
        schedules[testCase.endTime].isDateTimeBeforeDayPeriodEnd(testCase.dateTime());
    ASSERT_EQ(actualResult, testCase.beforeDayPeriodEndResult);
}

TEST_P(DateTimeMethodsTest, checkDateTimeFits)
{
    const DateTimeTestCase& testCase = GetParam();
    const bool actualResult = schedules[testCase.endTime].dateTimeFits(testCase.dateTime());
    ASSERT_EQ(actualResult, testCase.dateTimeFitsResult);
}

INSTANTIATE_TEST_CASE_P(QnServerBackupSchedule, DateTimeMethodsTest,
    ::testing::ValuesIn(kDateTimeTestCases), dateTimeTestCaseName);

} // namespace nx::core::resource::test
