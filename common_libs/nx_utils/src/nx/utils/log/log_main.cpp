#include "log_main.h"

namespace nx {
namespace utils {
namespace log {

namespace {

class LoggerCollection
{
public:
    LoggerCollection():
        m_mainLogger(std::make_shared<Logger>(
            kDefaultLevel, /*writer*/ nullptr, [this](){ onLevelChanged(); }))
    {
        updateMaxLevel();
    }

    std::shared_ptr<Logger> main()
    {
        QnMutexLocker lock(&m_mutex);
        return m_mainLogger;
    }

    std::shared_ptr<Logger> add(const std::set<Tag>& filters)
    {
        QnMutexLocker lock(&m_mutex);
        const auto logger = std::make_shared<Logger>(
            Level::none, /*writer*/ nullptr, [this](){ onLevelChanged(); });
        for(auto& f: filters)
            m_loggersByTags.emplace(f, logger);

        updateMaxLevel();
        return logger;
    }

    std::shared_ptr<Logger> get(const Tag& tag, bool exactMatch) const
    {
        QnMutexLocker lock(&m_mutex);
        if (exactMatch)
        {
            const auto it = m_loggersByTags.find(tag);
            return (it == m_loggersByTags.end()) ? std::shared_ptr<Logger>() : it->second;
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
    std::shared_ptr<Logger> m_mainLogger;
    std::map<Tag, std::shared_ptr<Logger>> m_loggersByTags;
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

} // namespace

std::shared_ptr<Logger> mainLogger()
{
    return loggerCollection()->main();
}

std::shared_ptr<Logger> addLogger(const std::set<Tag>& filters)
{
    return loggerCollection()->add(filters);
}

std::shared_ptr<Logger> getLogger(const Tag& tag)
{
    return loggerCollection()->get(tag, /*exactMatch*/ false);
}

std::shared_ptr<Logger> getExactLogger(const Tag& tag)
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
