// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "log_logger.h"

#include <cstdlib>

#include <QtCore/QDateTime>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    #include <sys/types.h>
    #include <sys/syscall.h>
    #include <unistd.h>
    static QString thisThreadId() { return QString::number(syscall(__NR_gettid)); }
#else
    #include <QtCore/QThread>
    static QString thisThreadId() { return QString::number((qint64) QThread::currentThreadId(), 16); }
#endif

#include <nx/utils/string.h>
#include <nx/utils/thread/mutex_delegate_factory.h>

#include <nx/build_info.h>

namespace {

using namespace nx::log;

static const std::array<QString, kLevelsCount> kLevels =
    nx::reflect::enumeration::visitAllItems<Level>(
        [](auto&&... items)
        {
            return std::array<QString, kLevelsCount>{
                toString(items.value).toUpper()...};
        });

template<typename... Items, size_t... N>
size_t findIndex(Level level, std::index_sequence<N...>, Items&&... items)
{
    size_t index = 0;
    bool found = false;
    found = ((items.value == level ? (found = true, index = N, found) : found) || ... || false);
    if (!found)
        return kLevels.size() - 1;
    return index;
}

const QString& findLevel(Level level)
{
    const auto idx = nx::reflect::enumeration::visitAllItems<Level>(
        [level](auto&&... items)
        {
            return findIndex(level, std::make_index_sequence<sizeof...(items)>(), items...);
        });
    return kLevels[idx];
}
} // namespace

namespace nx::log {

QString Logger::OneSecondCache::now()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto onlySec = duration_cast<seconds>(now.time_since_epoch());
    const auto onlyMsec = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
    QString dateTime;

    if (std::abs((onlySec - m_lastTimestamp).count()) >= 1)
    {
        m_cachedDateTime = QDateTime::fromSecsSinceEpoch(onlySec.count())
            .toString("yyyy-MM-dd HH:mm:ss.%1");
        m_lastTimestamp = onlySec;
    }
    return m_cachedDateTime.arg(onlyMsec, 3, 10, QChar('0'));
}

Logger::Logger(
    std::set<Filter> filters,
    Level defaultLevel,
    std::unique_ptr<AbstractWriter> writer)
    :
    m_mutex(nx::Mutex::Recursive),
    m_hardFilters(std::move(filters))
{
    if (writer)
        m_writer = std::move(writer);

    setDefaultLevel(defaultLevel);
}

std::set<Filter> Logger::filters() const
{
    return m_hardFilters;
}

void Logger::log(Level level, const Tag& tag, const QString& message)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!isToBeLogged(level, tag))
        return;

    logForced(level, tag, message);
}

void Logger::logForced(Level level, const Tag& tag, const QString& message)
{
    auto lock = m_dateTimeCache.lock();
    auto dateTime = lock->now();

    static const QString kTemplate = QLatin1String("%1 %2 %3 %4: %5");
    const auto output = kTemplate
        .arg(dateTime).arg(thisThreadId(), 6)
        .arg(findLevel(level), 7, QLatin1Char(' '))
        .arg(tag.toString()).arg(message);

    if (m_writer)
    {
        m_writer->write(level, output);
    }
    else
    {
        static StdOut stdOut;
        stdOut.write(level, output);
    }
}

bool Logger::isToBeLogged(Level level, const Tag& tag)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    for (const auto& filter: m_levelFilters)
    {
        if (filter.first.accepts(tag))
            return level <= filter.second;
    }

    return level <= m_defaultLevel;
}

Level Logger::defaultLevel() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_defaultLevel;
}

void Logger::setDefaultLevel(Level level)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (level == Level::trace)
        level = Level::verbose;
    m_defaultLevel = level;
    handleLevelChange(&lock);
}

LevelFilters Logger::levelFilters() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_levelFilters;
}

void Logger::setLevelFilters(LevelFilters filters)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_levelFilters = std::move(filters);
    handleLevelChange(&lock);
}

Level Logger::maxLevel() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    Level maxLevel = m_defaultLevel;
    for (const auto& element: m_levelFilters)
         maxLevel = std::max(maxLevel, element.second);
    return maxLevel;
}

void Logger::setSettings(const LoggerSettings& loggerSettings)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_settings = loggerSettings;

    if (auto file = dynamic_cast<File*>(m_writer.get()); file)
    {
        File::Settings fileSettings;
        fileSettings.maxFileTimePeriodS = m_settings.maxFileTimePeriodS;
        fileSettings.maxFileSizeB = m_settings.maxFileSizeB;
        fileSettings.maxVolumeSizeB = m_settings.maxVolumeSizeB;
        fileSettings.archivingEnabled = m_settings.archivingEnabled;
        file->setSettings(fileSettings);

        log(Level::info, this,
            nx::format("Log file size: %1, log volume size: %2, time period: %3, file: %4").args(
                nx::utils::bytesToString(m_settings.maxFileSizeB),
                nx::utils::bytesToString(m_settings.maxVolumeSizeB),
                m_settings.maxFileTimePeriodS,
                file->getFileName()));
    }
    if (m_settings.level.primary == Level::trace)
        m_settings.level.primary = Level::verbose;
    m_defaultLevel = m_settings.level.primary;
    m_levelFilters = m_settings.level.filters;
    handleLevelChange(&lock);
}

void Logger::setApplicationName(const QString& applicationName)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_applicationName = applicationName;
}

void Logger::setBinaryPath(const QString& binaryPath)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_binaryPath = binaryPath;
}

void Logger::setWriter(std::unique_ptr<AbstractWriter> writer)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_writer = std::move(writer);
}

void Logger::setOnLevelChanged(OnLevelChanged onLevelChanged)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_onLevelChanged = std::move(onLevelChanged);
}

std::optional<QString> Logger::filePath() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (const auto file = dynamic_cast<File*>(m_writer.get()); file)
        return file->getFileName();

    return std::nullopt;
}

cf::future<cf::unit> Logger::stopArchivingAsync()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_writer)
        return m_writer->stopArchivingAsync();
    else
        return cf::make_ready_future(cf::unit());
}

void Logger::writeLogHeader()
{
    const nx::log::Tag kStart(QLatin1String("START"));
    const auto write = [&](const QString& message) { log(Level::info, kStart, message); };
    write(QByteArray(80, '='));
    write(nx::format("%1 started, version: %2, revision: %3").args(
        m_applicationName,
        nx::build_info::vmsVersion(),
        build_info::revision()));

    if (!m_binaryPath.isEmpty())
        write(nx::format("Binary path: %1").arg(m_binaryPath));

    const auto filePath = this->filePath();
    write(nx::format("Log level: %1").arg(m_settings.level));
    write(nx::format("Log file size: %1, log volume size: %2, time period: %3, file: %4").args(
        nx::utils::bytesToString(m_settings.maxFileSizeB),
        nx::utils::bytesToString(m_settings.maxVolumeSizeB),
        m_settings.maxFileTimePeriodS,
        filePath ? *filePath : QString("-")));

    write(nx::format("Mutex implementation: %1").args(mutexImplementation()));
}

void Logger::handleLevelChange(nx::Locker<nx::Mutex>* lock) const
{
    decltype(m_onLevelChanged) onLevelChanged = m_onLevelChanged;
    nx::Unlocker<nx::Mutex> unlock(lock);
    if (onLevelChanged)
        onLevelChanged();
}

} // namespace nx::log
