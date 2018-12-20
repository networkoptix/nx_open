#include "time.h"

#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>

#include <nx/utils/log/log.h>
#include <nx/utils/unused.h>

#if defined(Q_OS_LINUX)
    #include <time.h>
    #include <sys/time.h>
    #include <QtCore/QFile>
    #include <QtCore/QProcess>
    #include <nx/utils/app_info.h>
#endif

using namespace std::chrono;

static_assert(
    std::chrono::system_clock::period::den / std::chrono::system_clock::period::num >= 1000,
    "Insufficient std::chrono::system_clock resolution.");

namespace nx {
namespace utils {

namespace {

static milliseconds utcTimeShift(0);
static milliseconds monotonicTimeShift(0);

} // namespace

// For tag in logging
struct TimeFunctionTag{};

/**
 * On Linux, get filename of a file which is used to set system time zone.
 * @param timeZoneId IANA id of a time zone.
 * @return On Linux - time zone file name, or a null string if time zone id is not valid; on
 *     other platforms - an empty string.
 */
static QString getTimeZoneFile(const QString& timeZoneId)
{
    #if defined(Q_OS_LINUX)
        QString timeZoneFile = QString("/usr/share/zoneinfo/%1").arg(timeZoneId);
        if (!QFile::exists(timeZoneFile))
            return QString();
        return timeZoneFile;
    #else
        nx::utils::unused(timeZoneId);
        return "";
    #endif
}

//-------------------------------------------------------------------------------------------------

system_clock::time_point utcTime()
{
    return system_clock::now() + utcTimeShift;
}

seconds timeSinceEpoch()
{
    return seconds(system_clock::to_time_t(utcTime()));
}

std::chrono::milliseconds millisSinceEpoch()
{
    return duration_cast<milliseconds>(utcTime().time_since_epoch());
}

steady_clock::time_point monotonicTime()
{
    return steady_clock::now() + monotonicTimeShift;
}

bool setTimeZone(const QString& timeZoneId)
{
    #if defined(Q_OS_LINUX)
        const QString& timeZoneFile = getTimeZoneFile(timeZoneId);
        if (timeZoneFile.isNull())
        {
            NX_ERROR(typeid(TimeFunctionTag), lm("setTimeZone(): Unsupported time zone id %1").arg(timeZoneId));
            return false;
        }

        if (unlink("/etc/localtime") != 0)
        {
            NX_ERROR(typeid(TimeFunctionTag), lm("setTimeZone(): Unable to delete /etc/localtime"));
            return false;
        }
        if (symlink(timeZoneFile.toLatin1().data(), "/etc/localtime") != 0)
        {
            NX_ERROR(typeid(TimeFunctionTag), lm("setTimeZone(): Unable to create symlink /etc/localtime"));
            return false;
        }
        QFile tzFile("/etc/timezone");
        if (!tzFile.open(QFile::WriteOnly | QFile::Truncate))
        {
            NX_ERROR(typeid(TimeFunctionTag), lm("setTimeZone(): Unable to rewrite /etc/timezone"));
            return false;
        }
        if (tzFile.write(timeZoneId.toLatin1()) <= 0)
        {
            NX_ERROR(typeid(TimeFunctionTag), lm("setTimeZone(): Unable to write time zone id to /etc/localtime"));
            return false;
        }

        return true;
    #else
        NX_ERROR(typeid(TimeFunctionTag), lm("setTimeZone(): Unsupported platform"));
        nx::utils::unused(timeZoneId);
        return false;
    #endif
}

QStringList getSupportedTimeZoneIds()
{
    QStringList result;

    for (const QByteArray& timeZoneId: QTimeZone::availableTimeZoneIds())
    {
        if (getTimeZoneFile(timeZoneId).isNull())
            continue;

        result.append(QString::fromLatin1(timeZoneId));
    }

    return result;
}

QString getCurrentTimeZoneId()
{
    const QString id = QDateTime::currentDateTime().timeZone().id();

    #if defined(Q_OS_LINUX)
        if (id.isEmpty())
        {
            // Obtain time zone via POSIX functions (thread-safe).
            NX_DEBUG(typeid(TimeFunctionTag), lm(
                "getCurrentTimeZoneId(): QDateTime time zone id is empty, trying localtime_r()"));
            constexpr int kMaxTimeZoneSize = 32;
            struct timespec timespecTime;
            struct tm tmTime;
            time(&timespecTime.tv_sec);
            localtime_r(&timespecTime.tv_sec, &tmTime);
            char timeZone[kMaxTimeZoneSize];
            strftime(timeZone, sizeof(timeZone), "%Z", &tmTime);
            return QLatin1String(timeZone);
        }
    #endif

    // For certain values, return the equivalent known to be in the list of supported ids.
    if (id == "Etc/UTC" ||
        id == "Etc/GMT" ||
        id == "Etc/GMT0" ||
        id == "Etc/GMT-0" ||
        id == "Etc/GMT+0" ||
        id == "Etc/Greenwich" ||
        id == "Etc/UCT" ||
        id == "Etc/Universal" ||
        id == "Etc/Zulu")
    {
        NX_DEBUG(typeid(TimeFunctionTag), lm("getCurrentTimeZoneId(): Converting %1 -> UTC").arg(id));
        return "UTC";
    }

    return id;
}

bool setDateTime(qint64 millisecondsSinceEpoch)
{
    #if defined(Q_OS_LINUX)
        struct timeval tv;
        tv.tv_sec = millisecondsSinceEpoch / 1000;
        tv.tv_usec = (millisecondsSinceEpoch % 1000) * 1000;
        if (settimeofday(&tv, 0) != 0)
        {
            NX_ERROR(typeid(TimeFunctionTag), lm("setDateTime(): settimeofday() failed"));
            return false;
        }

        if (AppInfo::isBpi() || AppInfo::isNx1())
        {
            // On NX1, we have to execute "hwclock -w" to save the time, and this command sometimes
            // failes, hence several attempts.
            for (int i = 0; i < 3; ++i)
            {
                if (QProcess::execute("hwclock -w") == 0)
                    return true;
            }
            NX_ERROR(typeid(TimeFunctionTag), lm("setDateTime(): \"hwclock -w\" fails"));
            return false;
        }

        return true;
    #else
        nx::utils::unused(millisecondsSinceEpoch);
        NX_ERROR(typeid(TimeFunctionTag), lm("setDateTime(): unsupported platform"));
    #endif

    return false;
}

NX_UTILS_API QDateTime fromOffsetSinceEpoch(const nanoseconds& offset)
{
    return QDateTime::fromMSecsSinceEpoch(
        duration_cast<milliseconds>(offset).count());
}

namespace test {

void ScopedTimeShift::shiftCurrentTime(ClockType clockType, milliseconds diff)
{
    if (clockType == ClockType::system)
        utcTimeShift = diff;
    else if (clockType == ClockType::steady)
        monotonicTimeShift = diff;
}

} // namespace test

} // namespace utils
} // namespace nx
