#include <atomic>

#include "log_main.h"

namespace nx {
namespace utils {
namespace log {

namespace {

class LoggerCollection
{
public:
    LoggerCollection()
    {
        m_mainLogger = std::make_unique<Logger>(std::set<Filter>(), kDefaultLevel);
        m_mainLogger->setOnLevelChanged([this]() { onLevelChanged(); });
        updateMaxLevel();
    }

    std::shared_ptr<AbstractLogger> main()
    {
        QnMutexLocker lock(&m_mutex);
        return m_mainLogger;
    }

    void setMainLogger(std::unique_ptr<AbstractLogger> logger)
    {
        if (!logger)
            return;

        logger->writeLogHeader();

        QnMutexLocker lock(&m_mutex);
        m_mainLogger = std::move(logger);
        m_mainLogger->setOnLevelChanged([this]() { onLevelChanged(); });

        updateMaxLevel();
    }

    void add(std::unique_ptr<AbstractLogger> logger)
    {
        if (!logger)
            return;

        QnMutexLocker lock(&m_mutex);

        std::shared_ptr<AbstractLogger> sharedLogger(std::move(logger));

        sharedLogger->setOnLevelChanged([this]() { onLevelChanged(); });
        for (auto& f: sharedLogger->filters())
            m_loggersByFilters.emplace(f, sharedLogger);

        updateMaxLevel();
    }

    std::shared_ptr<AbstractLogger> get(const Tag& tag, bool exactMatch) const
    {
        QnMutexLocker lock(&m_mutex);
        if (exactMatch)
        {
            const Filter filter(tag);
            auto it = m_loggersByFilters.find(filter);
            return it == m_loggersByFilters.cend()
                ? m_mainLogger
                : it->second;
        }
		
        for (auto& it: m_loggersByFilters)
        {
            if (it.first.accepts(tag))
                return it.second;
        }

        return m_mainLogger;
    }

    void remove(const std::set<Filter>& filters)
    {
        QnMutexLocker lock(&m_mutex);
        for (const auto f: filters)
            m_loggersByFilters.erase(f);

        updateMaxLevel();
    }

    Level maxLevel() const { return m_maxLevel; }

private:
    void updateMaxLevel();

    void onLevelChanged()
    {
        QnMutexLocker lock(&m_mutex);
        updateMaxLevel();
    }

private:
    mutable QnMutex m_mutex;
    std::shared_ptr<AbstractLogger> m_mainLogger;
    std::map<Filter, std::shared_ptr<AbstractLogger>> m_loggersByFilters;
    std::atomic<Level> m_maxLevel{Level::none};
};

void LoggerCollection::updateMaxLevel()
{
    m_maxLevel = m_mainLogger->maxLevel();
    for (const auto& element: m_loggersByFilters)
        m_maxLevel = std::max(element.second->maxLevel(), m_maxLevel.load());
}

static LoggerCollection* loggerCollection()
{
    static LoggerCollection collection;
    return &collection;
}

static std::atomic<bool> s_isConfigurationLocked = false;

} // namespace

std::shared_ptr<AbstractLogger> mainLogger()
{
    return loggerCollection()->main();
}

bool setMainLogger(std::unique_ptr<AbstractLogger> logger)
{
    if (s_isConfigurationLocked)
        return false;

    loggerCollection()->setMainLogger(std::move(logger));
    return true;
}

bool addLogger(
    std::unique_ptr<AbstractLogger> logger,
    bool writeLogHeader)
{
    if (s_isConfigurationLocked)
        return false;

    if (writeLogHeader)
        logger->writeLogHeader();

    loggerCollection()->add(std::move(logger));
    return true;
}

void lockConfiguration()
{
    s_isConfigurationLocked = true;
}

void unlockConfiguration()
{
    s_isConfigurationLocked = false;
}

std::shared_ptr<AbstractLogger> getLogger(const Tag& tag)
{
    return loggerCollection()->get(tag, /*exactMatch*/ false);
}

std::shared_ptr<AbstractLogger> getExactLogger(const Tag& tag)
{
    return loggerCollection()->get(tag, /*exactMatch*/ true);
}

void removeLoggers(const std::set<Filter>& filters)
{
    loggerCollection()->remove(filters);
}

Level maxLevel()
{
    return loggerCollection()->maxLevel();
}

bool isToBeLogged(Level level, const Tag& tag)
{
    return getLogger(tag)->isToBeLogged(level, tag);
}

} // namespace log
} // namespace utils
} // namespace nx
