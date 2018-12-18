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

#include <nx/utils/app_info.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/mutex_delegate_factory.h>

namespace nx {
namespace utils {
namespace log {

Logger::Logger(
    const std::set<Tag>& tags,
    Level defaultLevel,
    std::unique_ptr<AbstractWriter> writer)
    :
    m_mutex(QnMutex::Recursive),
    m_tags(tags),
    m_defaultLevel(defaultLevel)
{
    if (writer)
        m_writers.push_back(std::move(writer));
}

std::set<Tag> Logger::tags() const
{
    return m_tags;
}

void Logger::log(Level level, const Tag& tag, const QString& message)
{
    QnMutexLocker lock(&m_mutex);
    if (!isToBeLogged(level, tag))
        return;

    logForced(level, tag, message);
}

void Logger::logForced(Level level, const Tag& tag, const QString& message)
{
    QnMutexLocker lock(&m_mutex);
    static const QString kTemplate = QLatin1String("%1 %2 %3 %4: %5");
    const auto output = kTemplate
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")).arg(thisThreadId(), 6)
        .arg(toString(level).toUpper(), 7, QLatin1Char(' ')).arg(tag.toString()).arg(message);

    for (auto& writer: m_writers)
        writer->write(level, output);

    if (m_writers.empty())
    {
        static StdOut stdOut;
        stdOut.write(level, output);
    }
}

bool Logger::isToBeLogged(Level level, const Tag& tag)
{
    QnMutexLocker lock(&m_mutex);
    for (const auto& filter: m_levelFilters)
    {
        if (tag.matches(filter.first))
            return level <= filter.second;
    }

    return level <= m_defaultLevel;
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
    handleLevelChange(&lock);
}

LevelFilters Logger::levelFilters() const
{
    QnMutexLocker lock(&m_mutex);
    return m_levelFilters;
}

void Logger::setLevelFilters(LevelFilters filters)
{
    QnMutexLocker lock(&m_mutex);
    m_levelFilters = std::move(filters);
    handleLevelChange(&lock);
}

Level Logger::maxLevel() const
{
    QnMutexLocker lock(&m_mutex);
    Level maxLevel = m_defaultLevel;
    for (const auto& element: m_levelFilters)
         maxLevel = std::max(maxLevel, element.second);
    return maxLevel;
}

void Logger::setSettings(const LoggerSettings& loggerSettings)
{
    QnMutexLocker lock(&m_mutex);

    m_settings = loggerSettings;
    for (const auto[tag, level]: m_settings.level.filters)
        m_tags.insert(tag);
}

void Logger::setApplicationName(const QString& applicationName)
{
    QnMutexLocker lock(&m_mutex);
    m_applicationName = applicationName;
}

void Logger::setBinaryPath(const QString& binaryPath)
{
    QnMutexLocker lock(&m_mutex);
    m_binaryPath = binaryPath;
}

void Logger::setWriter(std::unique_ptr<AbstractWriter> writer)
{
    QnMutexLocker lock(&m_mutex);
    m_writers.clear();
    m_writers.push_back(std::move(writer));
}

void Logger::setOnLevelChanged(OnLevelChanged onLevelChanged)
{
    QnMutexLocker lock(&m_mutex);
    m_onLevelChanged = std::move(onLevelChanged);
}

std::optional<QString> Logger::filePath() const
{
    QnMutexLocker lock(&m_mutex);
    for (const auto& writer: m_writers)
    {
        if (const auto file = dynamic_cast<File*>(writer.get()))
            return file->makeFileName();
    }

    return std::nullopt;
}

void Logger::writeLogHeader()
{
    const nx::utils::log::Tag kStart(QLatin1String("START"));
    const auto write = [&](const Message& message) { log(Level::always, kStart, message); };
    write(QByteArray(80, '='));
    write(lm("%1 started, version: %2, revision: %3").args(
        m_applicationName, AppInfo::applicationVersion(), AppInfo::applicationRevision()));

    if (!m_binaryPath.isEmpty())
        write(lm("Binary path: %1").arg(m_binaryPath));

    const auto filePath = this->filePath();
    write(lm("Log level: %1").arg(m_settings.level));
    write(lm("Log file size: %2, backup count: %3, file: %4").args(
        nx::utils::bytesToString(m_settings.maxFileSize), m_settings.maxBackupCount,
        filePath ? *filePath : QString("-")));

    write(lm("Mutex implementation: %1").args(nx::utils::mutexImplementation()));
}

void Logger::handleLevelChange(QnMutexLockerBase* lock) const
{
    decltype(m_onLevelChanged) onLevelChanged = m_onLevelChanged;
    QnMutexUnlocker unlock(lock);
    if (onLevelChanged)
        onLevelChanged();
}

} // namespace log
} // namespace utils
} // namespace nx
