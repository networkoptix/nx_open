#include "log_logger.h"

#include <QtCore/QDateTime>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    #include <sys/types.h>
    #include <linux/unistd.h>
    static QString thisThreadId() { return QString::number(syscall(__NR_gettid), 6); }
#else
    #include <QtCore/QThread>
    static QString thisThreadId() { return QString::number((qint64) QThread::currentThreadId(), 6, 16); }
#endif

namespace nx {
namespace utils {
namespace log {

Level levelFromString(const QString& levelString)
{
    const auto level = levelString.toLower();
    if (level == QLatin1String("always"))
        return Level::always;

    if (level == QLatin1String("error"))
        return Level::error;

    if (level == QLatin1String("warning"))
        return Level::warning;

    if (level == QLatin1String("info"))
        return Level::info;

    if (level == QLatin1String("debug"))
        return Level::debug;

    if (level == QLatin1String("verbose"))
        return Level::verbose;

    return Level::none;
}

QString toString(Level level)
{
    switch (level)
    {
        case Level::none:
            return QLatin1String("none");

        case Level::always:
            return QLatin1String("always");

        case Level::error:
            return QLatin1String("error");

        case Level::warning:
            return QLatin1String("warning");

        case Level::info:
            return QLatin1String("info");

        case Level::debug:
            return QLatin1String("debug");

        case Level::verbose:
            return QLatin1String("verbose");
    };

    NX_ASSERT(false, lm("Unknown level: %1").arg(static_cast<int>(level)));
    return QLatin1String("unknown");
}

Logger::Logger(Level level, std::unique_ptr<AbstractWriter> writer):
    m_mutex(QnMutex::Recursive),
    m_defaultLevel(level)
{
    if (writer)
        m_writers.push_back(std::move(writer));
}

void Logger::log(Level level, const QString& tag, const QString& message)
{
    QnMutexLocker lock(&m_mutex);
    if (!isToBeLogged(level, tag))
        return;

    static const QString kTemplate = QLatin1String("%1 %2 %3 %4: %5");
    const auto output = kTemplate
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")).arg(thisThreadId())
        .arg(toString(level).toUpper(), 7, QLatin1Char(' ')).arg(tag).arg(message);

    for (auto& writer: m_writers)
        writer->write(output);

    if (m_writers.empty())
    {
        static StdOut stdOut;
        stdOut.write(output);
    }
}

bool Logger::isToBeLogged(Level level, const QString& tag)
{
    QnMutexLocker lock(&m_mutex);
    if (static_cast<int>(level) <= static_cast<int>(m_defaultLevel))
        return true;

    for (const auto& filter: m_exceptionFilters)
    {
        if (tag.startsWith(filter))
            return true;
    }

    return false;
}

Level Logger::defaultLevel()
{
    QnMutexLocker lock(&m_mutex);
    return m_defaultLevel;
}

void Logger::setDefaultLevel(Level level)
{
    QnMutexLocker lock(&m_mutex);
    m_defaultLevel = level;
}

std::set<QString> Logger::exceptionFilters()
{
    QnMutexLocker lock(&m_mutex);
    return m_exceptionFilters;
}

void Logger::setExceptionFilters(std::set<QString> filters)
{
    QnMutexLocker lock(&m_mutex);
    m_exceptionFilters = std::move(filters);
}

void Logger::setWriters(std::vector<std::unique_ptr<AbstractWriter>> writers)
{
    QnMutexLocker lock(&m_mutex);
    m_writers = std::move(writers);
}

} // namespace log
} // namespace utils
} // namespace nx
