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

    std::shared_ptr<Logger> get(const QString& tag)
    {
        QnMutexLocker lock(&m_mutex);
        const auto it = m_loggersByTags.lower_bound(tag);
        if (it != m_loggersByTags.end() && tag.startsWith(it->first))
            return it->second;
        else
            return m_mainLogger;
    }

private:
    QnMutex m_mutex;
    std::shared_ptr<Logger> m_mainLogger;
    std::map<QString, std::shared_ptr<Logger>> m_loggersByTags;
};

static LoggerCollection* loggerCollection()
{
    static LoggerCollection collection;
    return &collection;
}

} // namespace

std::shared_ptr<Logger> main()
{
    return loggerCollection()->main();
}

std::shared_ptr<Logger> add(const std::set<QString>& filters)
{
    return loggerCollection()->add(filters);
}

std::shared_ptr<Logger> get(const QString& tag)
{
    return loggerCollection()->get(tag);
}

} // namespace log
} // namespace utils
} // namespace nx
