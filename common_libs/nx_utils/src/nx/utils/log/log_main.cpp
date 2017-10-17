#include "log_main.h"

namespace nx {
namespace utils {
namespace log {

namespace {

class LoggerCollection
{
public:
    LoggerCollection():
        m_mainLogger(std::make_shared<Logger>([this]() { updateMaxLevel(); }))
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
        const auto logger = std::make_shared<Logger>([this]() { updateMaxLevel(); });
        for(auto& f: filters)
            m_loggersByTags.emplace(f, logger);

        updateMaxLevel();
        return logger;
    }

    std::shared_ptr<Logger> get(const Tag& tag, bool allowMainLogger) const
    {
        QnMutexLocker lock(&m_mutex);
        for (auto& it: m_loggersByTags)
        {
            if (tag.matches(it.first))
                return it.second;
        }

        return allowMainLogger ? m_mainLogger : std::shared_ptr<Logger>();
    }

    void remove(const std::set<Tag>& filters)
    {
        QnMutexLocker lock(&m_mutex);
        for (const auto f: filters)
            m_loggersByTags.erase(f);

        updateMaxLevel();
    }

    Level maxLevel() const;

private:
    void updateMaxLevel();

private:
    mutable QnMutex m_mutex;
    std::shared_ptr<Logger> m_mainLogger;
    std::map<Tag, std::shared_ptr<Logger>> m_loggersByTags;
    Level m_maxLevel = Level::none;
};

Level LoggerCollection::maxLevel() const
{
    return m_maxLevel;
}

void LoggerCollection::updateMaxLevel()
{
    m_maxLevel = m_mainLogger->maxLevel();
    for (const auto& element: m_loggersByTags)
    {
        const auto& logger = element.second;
        if (logger->maxLevel() > m_maxLevel)
            m_maxLevel = logger->maxLevel();
    }
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

std::shared_ptr<Logger> getLogger(const Tag& tag, bool allowMainLogger)
{
    return loggerCollection()->get(tag, allowMainLogger);
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

namespace detail {

Helper::Helper(Level level, Tag tag):
    m_level(level),
    m_tag(std::move(tag))
{
    m_logger = getLogger(m_tag);
    if (!m_logger->isToBeLogged(m_level, m_tag))
        m_logger.reset();
}

void Helper::log(const QString& message)
{
    m_logger->logForced(m_level, m_tag, message);
}

Helper::operator bool() const
{
    return (bool) m_logger;
}

Stream::~Stream()
{
    if (!m_logger)
        return;

    NX_EXPECT(!m_strings.isEmpty());
    log(m_strings.join(' '));
}

Stream::operator bool() const
{
    return !(bool) m_logger;
}

} // namespace detail

} // namespace log
} // namespace utils
} // namespace nx
