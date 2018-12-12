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
        m_mainLogger = std::make_unique<Logger>(std::set<Tag>(), kDefaultLevel);
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
        for (auto& f: sharedLogger->tags())
            m_loggersByTags.emplace(f, sharedLogger);

        updateMaxLevel();
    }

    std::shared_ptr<AbstractLogger> get(const Tag& tag, bool exactMatch) const
    {
        QnMutexLocker lock(&m_mutex);
        if (exactMatch)
        {
            const auto it = m_loggersByTags.find(tag);
            return (it == m_loggersByTags.end()) ? std::shared_ptr<AbstractLogger>() : it->second;
        }

        for (auto& it: m_loggersByTags)
        {
            if (tag.matches(it.first))
                return it.second;
        }

        return m_mainLogger;
    }

    void remove(const std::set<Tag>& filters)
    {
        QnMutexLocker lock(&m_mutex);
        for (const auto f: filters)
            m_loggersByTags.erase(f);

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
    std::map<Tag, std::shared_ptr<AbstractLogger>> m_loggersByTags;
    std::atomic<Level> m_maxLevel{Level::none};
};

void LoggerCollection::updateMaxLevel()
{
    m_maxLevel = m_mainLogger->maxLevel();
    for (const auto& element: m_loggersByTags)
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

void removeLoggers(const std::set<Tag>& filters)
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
