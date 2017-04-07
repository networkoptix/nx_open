#include "log_logger.h"

#include <QtCore/QDateTime>

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

Logger::Logger(Level level, std::vector<std::unique_ptr<AbstractWriter>> writers):
    m_mutex(QnMutex::Recursive),
    m_defaultLevel(level),
    m_writers(std::move(writers))
{
}

void Logger::log(Level level, const QString& tag, const QString& message)
{
    QnMutexLocker lock(&m_mutex);
    if (!isToBeLogged(level, tag))
        return;

    static const QString kTemplate = QLatin1String("%1 %2 %3: %4");
    const auto output = kTemplate
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"))
        .arg(toString(level).toUpper(), 7, QLatin1Char(' ')).arg(message);

    for (auto& writer: m_writers)
        writer->write(output);
}

bool Logger::isToBeLogged(Level level, const QString& tag)
{
    QnMutexLocker lock(&m_mutex);
    if (static_cast<int>(level) < static_cast<int>(m_defaultLevel))
        return true;

    const auto it = m_exceptionFilters.lower_bound(tag);
    if (it != m_exceptionFilters.end() && tag.startsWith(*it))
        return true;

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
    m_defaultLevel = m_defaultLevel;
}

std::set<QString> Logger::exceptionFilters()
{
    QnMutexLocker lock(&m_mutex);
    return m_exceptionFilters;
}

void Logger::setExceptionFilters(std::set<QString> filters)
{
    QnMutexLocker lock(&m_mutex);
    m_exceptionFilters = filters;
}

void Logger::setWriters(std::vector<std::unique_ptr<AbstractWriter>> writers)
{
    QnMutexLocker lock(&m_mutex);
    m_writers = std::move(writers);
}

} // namespace log
} // namespace utils
} // namespace nx
