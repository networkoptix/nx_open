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
            Level::none, /*writer*/ nullptr, [this](){ onLevelChanged(); }))
    {
        updateMaxLevel();
    }

    std::shared_ptr<Logger> main()
    {
        QnMutexLocker lock(&m_mutex);
        return m_mainLogger;
    }

    std::shared_ptr<Logger> add(const std::set<QString>& filters)
    {
        QnMutexLocker lock(&m_mutex);
        const auto logger = std::make_shared<Logger>(
            Level::none, /*writer*/ nullptr, [this](){ onLevelChanged(); });
        for(auto& f: filters)
            m_loggersByTags.emplace(f, logger);

        updateMaxLevel();
        return logger;
    }

    std::shared_ptr<Logger> get(const QString& tag, bool allowMainLogger) const
    {
        QnMutexLocker lock(&m_mutex);
        for (auto& it: m_loggersByTags)
        {
            if (tag.startsWith(it.first))
                return it.second;
        }

        return allowMainLogger ? m_mainLogger : std::shared_ptr<Logger>();
    }

    void remove(const std::set<QString>& filters)
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
    std::map<QString, std::shared_ptr<Logger>> m_loggersByTags;
    std::atomic<Level> m_maxLevel = Level::none;
};

void LoggerCollection::updateMaxLevel()
{
    m_maxLevel = m_mainLogger->maxLevel();
    for (const auto& element: m_loggersByTags)
        m_maxLevel = (Level) std::max((int) element.second->maxLevel(), (int) m_maxLevel.load());
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

std::shared_ptr<Logger> addLogger(const std::set<QString>& filters)
{
    return loggerCollection()->add(filters);
}

std::shared_ptr<Logger> getLogger(const QString& tag, bool allowMainLogger)
{
    return loggerCollection()->get(tag, allowMainLogger);
}

void removeLoggers(const std::set<QString>& filters)
{
    loggerCollection()->remove(filters);
}

Level maxLevel()
{
    return loggerCollection()->maxLevel();
}

} // namespace log
} // namespace utils
} // namespace nx
