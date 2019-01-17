#include "logger_collection.h"

namespace nx {
namespace utils {
namespace log {

LoggerCollection::LoggerCollection()
{
    m_mainLogger = std::make_unique<Logger>(std::set<Tag>(), kDefaultLevel);
    m_mainLogger->setOnLevelChanged([this]() { onLevelChanged(); });
    updateMaxLevel();
}

LoggerCollection * LoggerCollection::instance()
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

int LoggerCollection::add(std::unique_ptr<AbstractLogger> logger)
{
    if (!logger)
        return invalidId;

    QnMutexLocker lock(&m_mutex);

    std::shared_ptr<AbstractLogger> sharedLogger(std::move(logger));

    sharedLogger->setOnLevelChanged([this]() { onLevelChanged(); });

    Context loggerContext(m_loggerId, sharedLogger);

    bool inserted = false;
    for (const auto& tag : sharedLogger->tags())
    {
        if (m_loggersByTags.emplace(tag, loggerContext).second)
            inserted = true;
    }

    updateMaxLevel();

    if (inserted)
    {
        ++m_loggerId;
        return loggerContext.id;
    }

    return invalidId;
}

std::shared_ptr<AbstractLogger> LoggerCollection::get(const Tag& tag, bool exactMatch) const
{
    QnMutexLocker lock(&m_mutex);
    if (exactMatch)
    {
        const auto it = m_loggersByTags.find(tag);
        return (it == m_loggersByTags.end()) ? nullptr : it->second.logger;
    }

    for (const auto& it : m_loggersByTags)
    {
        if (tag.matches(it.first))
            return it.second.logger;
    }

    return m_mainLogger;
}

std::shared_ptr<AbstractLogger> LoggerCollection::get(int loggerId) const
{
    QnMutexLocker lock(&m_mutex);
    for (const auto& element : m_loggersByTags)
    {
        if (element.second.id == loggerId)
            return element.second.logger;
    }

    return nullptr;
}

std::set<Tag> LoggerCollection::getEffectiveTags(int loggerId) const
{
    QnMutexLocker lock(&m_mutex);
    std::set<Tag> effectiveTags;

    for (const auto& element : m_loggersByTags)
    {
        if (element.second.id == loggerId)
            effectiveTags.emplace(element.first);
    }

    return effectiveTags;
}

LoggerCollection::LoggersByTag LoggerCollection::allLoggers() const
{
    QnMutexLocker lock(&m_mutex);
    return m_loggersByTags;
}

void LoggerCollection::remove(const std::set<Tag>& filters)
{
    QnMutexLocker lock(&m_mutex);
    for (const auto& f: filters)
        m_loggersByTags.erase(f);

    updateMaxLevel();
}

void LoggerCollection::remove(int loggerId)
{
    QnMutexLocker lock(&m_mutex);
    for (auto it = m_loggersByTags.begin(); it != m_loggersByTags.end();)
    {
        if (it->second.id == loggerId)
            it = m_loggersByTags.erase(it);
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
    for (const auto& element: m_loggersByTags)
        m_maxLevel = std::max(element.second.logger->maxLevel(), m_maxLevel.load());
}

void LoggerCollection::onLevelChanged()
{
    QnMutexLocker lock(&m_mutex);
    updateMaxLevel();
}

std::shared_ptr<AbstractLogger> LoggerCollection::getInternal(int loggerId) const
{
    for (const auto& element : m_loggersByTags)
    {
        if (element.second.id == loggerId)
            return element.second.logger;
    }

    return nullptr;
}

} // namespace nx
} // namespace utils
} // namespace log