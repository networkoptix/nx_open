#include "time.h"

#include <nx/utils/log/log.h>
#include <utils/common/unused.h>

#if defined(Q_OS_LINUX)
    #include <sys/time.h>
    #include <QtCore/QFile>
    #include <QtCore/QProcess>
    #include <utils/common/app_info.h>
#endif

using namespace std::chrono;

namespace nx {
namespace utils {

namespace {

static std::chrono::milliseconds utcTimeShift(0);
static std::chrono::milliseconds monotonicTimeShift(0);

} // namespace

std::chrono::system_clock::time_point utcTime()
{
    return system_clock::now() + utcTimeShift;
}

std::chrono::seconds timeSinceEpoch()
{
    return duration_cast<seconds>(utcTime().time_since_epoch());
}

std::chrono::steady_clock::time_point monotonicTime()
{
    return steady_clock::now() + monotonicTimeShift;
}

QString getTimeZoneFile(const QString& timeZoneId)
{
    #if defined(Q_OS_LINUX)
        QString timeZoneFile = lit("/usr/share/zoneinfo/%1").arg(timeZoneId);
        if (!QFile::exists(timeZoneFile))
            return QString();
        return timeZoneFile;
    #else
        QN_UNUSED(timeZoneId);
        return lit("");
    #endif
}

bool setTimeZone(const QString& timeZoneId)
{
    #if defined(Q_OS_LINUX)
        const QString& timeZoneFile = getTimeZoneFile(timeZoneId);
        if (timeZoneFile.isNull())
        {
            NX_LOG(lit("setTimeZone(): Invalid time zone id %1").arg(timeZoneId), cl_logERROR);
            return false;
        }

        if (unlink("/etc/localtime") != 0)
        {
            NX_LOG(lit("setTimeZone(): Unable to delete /etc/localtime"), cl_logERROR);
            return false;
        }
        if (symlink(timeZoneFile.toLatin1().data(), "/etc/localtime") != 0)
        {
            NX_LOG(lit("setTimeZone(): Unable to create symlink /etc/localtime"), cl_logERROR);
            return false;
        }
        QFile tzFile(lit("/etc/timezone"));
        if (!tzFile.open(QFile::WriteOnly | QFile::Truncate))
        {
            NX_LOG(lit("setTimeZone(): Unable to rewrite /etc/timezone"), cl_logERROR);
            return false;
        }
        if (tzFile.write(timeZoneId.toLatin1()) <= 0)
        {
            NX_LOG(lit("setTimeZone(): Unable to write time zone id to /etc/localtime"),
                cl_logERROR);
            return false;
        }

        return true;
    #else
        NX_LOG(lit("setTimeZone(): Unsupported platform"), cl_logERROR);
        QN_UNUSED(timeZoneId);
        return false;
    #endif
}

bool setDateTime(qint64 millisecondsSinceEpoch)
{
    #if defined(Q_OS_LINUX)
        struct timeval tv;
        tv.tv_sec = millisecondsSinceEpoch / 1000;
        tv.tv_usec = (millisecondsSinceEpoch % 1000) * 1000;
        if (settimeofday(&tv, 0) != 0)
        {
            NX_LOG(lit("setDateTime(): settimeofday() failed"), cl_logERROR);
            return false;
        }

        if (QnAppInfo::isBpi() || QnAppInfo::isNx1())
        {
            // On NX1, we have to execute "hwclock -w" to save the time, and this command sometimes
            // failes, hence several attempts.
            for (int i = 0; i < 3; ++i)
            {
                if (QProcess::execute(lit("hwclock -w")) == 0)
                    return true;
            }
            NX_LOG(lit("setDateTime(): \"hwclock -w\" fails"), cl_logERROR);
            return false;
        }

        return true;
    #else
        QN_UNUSED(millisecondsSinceEpoch);
        NX_LOG(lit("setDateTime(): unsupported platform"), cl_logERROR);
    #endif

    return true;
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
