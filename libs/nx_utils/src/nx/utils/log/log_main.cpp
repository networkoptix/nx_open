#include "log_main.h"

#include <atomic>

#include "logger_collection.h"

namespace nx {
namespace utils {
namespace log {

namespace {

LoggerCollection* loggerCollection()
{
    return LoggerCollection::instance();
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
