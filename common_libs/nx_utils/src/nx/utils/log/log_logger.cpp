#include "log_logger.h"

#include <QtCore/QDateTime>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    #include <sys/types.h>
    #include <linux/unistd.h>
    static QString thisThreadId() { return QString::number(syscall(__NR_gettid)); }
#else
    #include <QtCore/QThread>
    static QString thisThreadId() { return QString::number((qint64) QThread::currentThreadId(), 16); }
#endif

namespace nx {
namespace utils {
namespace log {

Logger::Logger(
    Level level,
    std::unique_ptr<AbstractWriter> writer,
    OnLevelChanged onLevelChanged)
    :
    m_mutex(QnMutex::Recursive),
    m_onLevelChanged(std::move(onLevelChanged)),
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

    logForced(level, tag, message);
}

void Logger::logForced(Level level, const QString& tag, const QString& message)
{
    QnMutexLocker lock(&m_mutex);
    static const QString kTemplate = QLatin1String("%1 %2 %3 %4: %5");
    const auto output = kTemplate
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")).arg(thisThreadId(), 6)
        .arg(toString(level).toUpper(), 7, QLatin1Char(' ')).arg(tag).arg(message);

    for (auto& writer: m_writers)
        writer->write(level, output);

    if (m_writers.empty())
    {
        static StdOut stdOut;
        stdOut.write(level, output);
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

Level Logger::defaultLevel() const
{
    QnMutexLocker lock(&m_mutex);
    return m_defaultLevel;
}

void Logger::setDefaultLevel(Level level)
{
    QnMutexLocker lock(&m_mutex);
    m_defaultLevel = level;
    if (m_onLevelChanged)
        m_onLevelChanged();
}

std::set<QString> Logger::exceptionFilters() const
{
    QnMutexLocker lock(&m_mutex);
    return m_exceptionFilters;
}

void Logger::setExceptionFilters(std::set<QString> filters)
{
    QnMutexLocker lock(&m_mutex);
    m_exceptionFilters = std::move(filters);
    if (m_onLevelChanged)
        m_onLevelChanged();
}

Level Logger::maxLevel() const
{
    QnMutexLocker lock(&m_mutex);
    if (!m_exceptionFilters.empty())
        return Level::verbose;
    else
        return m_defaultLevel;
}

void Logger::setWriters(std::vector<std::unique_ptr<AbstractWriter>> writers)
{
    QnMutexLocker lock(&m_mutex);
    m_writers = std::move(writers);
}

void Logger::setWriter(std::unique_ptr<AbstractWriter> writer)
{
    QnMutexLocker lock(&m_mutex);
    m_writers.clear();
    m_writers.push_back(std::move(writer));
}

boost::optional<QString> Logger::filePath() const
{
    QnMutexLocker lock(&m_mutex);
    for (const auto& writer: m_writers)
    {
        if (const auto file = dynamic_cast<File*>(writer.get()))
            return file->makeFileName();
    }

    return boost::none;
}

} // namespace log
} // namespace utils
} // namespace nx
