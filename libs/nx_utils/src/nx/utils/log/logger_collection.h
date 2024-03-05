// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/system_error.h>

#include "log_logger.h"

namespace nx::log {

class NX_UTILS_API LoggerCollection
{
public:
    static constexpr int kInvalidId = -1;

    struct Context
    {
        int id = kInvalidId;
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

    using LoggersByFilter = std::map<Filter, Context>;

public:
    LoggerCollection();
    ~LoggerCollection();

    static LoggerCollection* instance();

    std::shared_ptr<AbstractLogger> main();

    void setMainLogger(std::unique_ptr<AbstractLogger> logger);

    /**
     * Add a logger to the collection.
     * If this logger has a tag that is already associated with another logger, the existing
     * logger/tag association is not overwritten and the new logger will not be associated with
     * the tag. To get the tags associated with a given logger, see getEffectiveTags().
     *
     * @return The logger id if added successfully (at least one tag was not already taken),
     * -1 otherwise.
     */
    int add(std::shared_ptr<AbstractLogger> logger);

    std::shared_ptr<AbstractLogger> get(const Tag& tag, bool exactMatch) const;

    std::shared_ptr<AbstractLogger> get(int loggerId) const;

    /**
     * Get all the filters associated with the given logger id.
     * If multiple loggers are added with the same filter, only the first logger added is
     * associated with that filter.
     */
    std::set<Filter> getEffectiveFilters(int loggerId) const;

    /**
     * Get all the loggers in this collection.
     * If a logger has multiple tags associated with it, the data structure will contain the same
     * logger multiple times. there are as many loggers as there are tags associated with it.
     */
    LoggersByFilter allLoggers() const;

    void remove(const std::set<Filter>& filters);

    void remove(int loggerId);

    Level maxLevel() const;

    void stopArchiving();

private:
    void updateMaxLevel();

    void onLevelChanged();

private:
    /** Helps avoid crashing if a method is called during the static deinitialization phase. */
    std::atomic<bool> m_isDestroyed{false};

    mutable nx::Mutex m_mutex;
    std::shared_ptr<AbstractLogger> m_mainLogger;
    LoggersByFilter m_loggersByFilter;
    std::atomic<Level> m_maxLevel{Level::none};
    int m_loggerId = 0;
};

} // namespace nx::log
