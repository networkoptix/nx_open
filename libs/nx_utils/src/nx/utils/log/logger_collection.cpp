#include "logger_collection.h"

namespace nx {
namespace utils {
namespace log {

LoggerCollection::LoggerCollection()
{
    m_mainLogger = std::make_unique<Logger>(std::set<Filter>(), kDefaultLevel);
    m_mainLogger->setOnLevelChanged([this]() { onLevelChanged(); });
    updateMaxLevel();
}

LoggerCollection* LoggerCollection::instance()
{
    static LoggerCollection collection;
    return &collection;
}

std::shared_ptr<AbstractLogger> LoggerCollection::main()
{
    QnMutexLocker lock(&m_mutex);
    return m_mainLogger;
}

void LoggerCollection::setMainLogger(std::unique_ptr<AbstractLogger> logger)
{
    if (!logger)
        return;

    logger->writeLogHeader();

    QnMutexLocker lock(&m_mutex);
    m_mainLogger = std::move(logger);
    m_mainLogger->setOnLevelChanged([this]() { onLevelChanged(); });

    updateMaxLevel();
}

int LoggerCollection::add(std::shared_ptr<AbstractLogger> logger)
{
    if (!logger)
        return kInvalidId;

    QnMutexLocker lock(&m_mutex);

    logger->setOnLevelChanged([this]() { onLevelChanged(); });

    Context loggerContext(m_loggerId, logger);

    bool inserted = false;
    for (const auto& filter: logger->filters())
    {
        if (m_loggersByFilter.emplace(filter, loggerContext).second)
            inserted = true;
    }

    updateMaxLevel();

    if (inserted)
        return m_loggerId++;

    return kInvalidId;
}

std::shared_ptr<AbstractLogger> LoggerCollection::get(const Tag& tag, bool exactMatch) const
{
    QnMutexLocker lock(&m_mutex);
    for (auto& it: m_loggersByFilter)
    {
        if (exactMatch)
        {
            if (it.first.toString() == tag.toString())
                return it.second.logger;
        }
        else
        {
            if (it.first.accepts(tag))
                return it.second.logger;
        }
    }

    return m_mainLogger;
}

std::shared_ptr<AbstractLogger> LoggerCollection::get(int loggerId) const
{
    QnMutexLocker lock(&m_mutex);
    for (const auto& element : m_loggersByFilter)
    {
        if (element.second.id == loggerId)
            return element.second.logger;
    }

    return nullptr;
}

std::set<Filter> LoggerCollection::getEffectiveFilters(int loggerId) const
{
    QnMutexLocker lock(&m_mutex);
    std::set<Filter> effectiveFilters;

    for (const auto& element: m_loggersByFilter)
    {
        if (element.second.id == loggerId)
            effectiveFilters.emplace(element.first);
    }

    return effectiveFilters;
}

LoggerCollection::LoggersByFilter LoggerCollection::allLoggers() const
{
    QnMutexLocker lock(&m_mutex);
    return m_loggersByFilter;
}

void LoggerCollection::remove(const std::set<Filter>& filters)
{
    QnMutexLocker lock(&m_mutex);
    for (const auto& f: filters)
        m_loggersByFilter.erase(f);

    updateMaxLevel();
}

void LoggerCollection::remove(int loggerId)
{
    QnMutexLocker lock(&m_mutex);
    for (auto it = m_loggersByFilter.begin(); it != m_loggersByFilter.end();)
    {
        if (it->second.id == loggerId)
            it = m_loggersByFilter.erase(it);
        else
            ++it;
    }
}

Level LoggerCollection::maxLevel() const
{
    return m_maxLevel;
}

void LoggerCollection::updateMaxLevel()
{
    m_maxLevel = m_mainLogger->maxLevel();
    for (const auto& element: m_loggersByFilter)
        m_maxLevel = std::max(element.second.logger->maxLevel(), m_maxLevel.load());
}

void LoggerCollection::onLevelChanged()
{
    QnMutexLocker lock(&m_mutex);
    updateMaxLevel();
}

} // namespace nx
} // namespace utils
} // namespace log