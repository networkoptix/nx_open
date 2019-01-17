#pragma once

#include "log_logger.h"

#include <nx/utils/system_error.h>

namespace nx {
namespace utils {
namespace log {

class NX_UTILS_API LoggerCollection
{
public:
    struct Context
    {
        int id;
        std::shared_ptr<AbstractLogger> logger;

        Context(int id, const std::shared_ptr<AbstractLogger>& logger):
            id(id),
            logger(logger)
        {
        }

        bool operator <(const Context& rhs) const
        {
            return id < rhs.id;
        }
    };

    using LoggersByTag = std::map<Tag, Context>;

    static constexpr int invalidId = -1;

public:
    LoggerCollection();
    ~LoggerCollection() = default;

    static LoggerCollection * instance();

    std::shared_ptr<AbstractLogger> main();

    void setMainLogger(std::unique_ptr<AbstractLogger> logger);

    /**
     * Add a logger to the collection. 
     * If this logger has a tag that is already associated with another logger, the existing
     * logger/tag association is not overwritten and the new logger will not be associated with
     * the tag. To get the tags associated with a given logger, see getEffectiveTags().
     *
     * @return - The logger id if added successfully (at least one tag was not already taken), 
     * -1 otherwise.
     */
    int add(std::unique_ptr<AbstractLogger> logger);

    std::shared_ptr<AbstractLogger> get(const Tag& tag, bool exactMatch) const;

    std::shared_ptr<AbstractLogger> get(int loggerId) const;

    /**
     * Get the effective tags for a logger with the given id.
     * If multiple loggers are added with the same tag, only the first logger added is associated 
     * with the tag.
     */
    std::set<Tag> getEffectiveTags(int loggerId) const;

    /**
     * Get all the loggers in this collection.
     * If a logger has multiple tags associated with it, the data structure will contain the same 
     * logger multiple times. there are as many loggers as there are tags associated with it.
     */
    LoggersByTag allLoggers() const;

    void remove(const std::set<Tag>& filters);

    void remove(int loggerId);

    Level maxLevel() const;

private:
    void updateMaxLevel();

    void onLevelChanged();

    std::shared_ptr<AbstractLogger> getInternal(int loggerId) const;

private:
    mutable QnMutex m_mutex;
    std::shared_ptr<AbstractLogger> m_mainLogger;
    //std::map<Tag, std::shared_ptr<AbstractLogger>> m_loggersByTags;
    std::map<Tag, Context> m_loggersByTags;
    std::atomic<Level> m_maxLevel{Level::none};
    int m_loggerId = 0;
};

} // namespace log
} // namespace utils
} // namespace nx