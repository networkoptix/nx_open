#include "log_main.h"

namespace nx {
namespace utils {
namespace log {

namespace {

class LoggerCollection
{
public:
    LoggerCollection():
        m_mainLogger(std::make_shared<Logger>())
    {
    }

    std::shared_ptr<Logger> main()
    {
        QnMutexLocker lock(&m_mutex);
        return m_mainLogger;
    }

    std::shared_ptr<Logger> add(const std::set<QString>& filters)
    {
        QnMutexLocker lock(&m_mutex);
        const auto logger = std::make_shared<Logger>();
        for(auto& f: filters)
            m_loggersByTags.emplace(f, logger);

        return logger;
    }

    std::shared_ptr<Logger> get(const QString& tag, bool allowMain) const
    {
        QnMutexLocker lock(&m_mutex);
        for (auto& it: m_loggersByTags)
        {
            if (tag.startsWith(it.first))
                return it.second;
        }

        return allowMain ? m_mainLogger : std::shared_ptr<Logger>();
    }

    void remove(const std::set<QString>& filters)
    {
        QnMutexLocker lock(&m_mutex);
        for (const auto f: filters)
            m_loggersByTags.erase(f);
    }

private:
    mutable QnMutex m_mutex;
    std::shared_ptr<Logger> m_mainLogger;
    std::map<QString, std::shared_ptr<Logger>> m_loggersByTags;
};

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

std::shared_ptr<Logger> getLogger(const QString& tag, bool allowMain)
{
    return loggerCollection()->get(tag, allowMain);
}

void removeLoggers(const std::set<QString>& filters)
{
    loggerCollection()->remove(filters);
}

namespace detail {

Helper::Helper(Level level, const QString& tag):
    m_level(level),
    m_tag(tag)
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

Stream::Stream(Level level, const QString& tag):
    Helper(level, tag)
{
}

Stream::~Stream()
{
    if (m_logger)
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
