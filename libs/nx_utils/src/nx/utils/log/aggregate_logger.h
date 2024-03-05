// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include "abstract_logger.h"

namespace nx::log {

/**
 * Hides multiple loggers behind itself.
 */
class NX_UTILS_API AggregateLogger:
    public AbstractLogger
{
public:
    AggregateLogger(std::vector<std::unique_ptr<AbstractLogger>> loggers);
    AggregateLogger(AggregateLogger&&) = delete;

    virtual std::set<Filter> filters() const override;

    virtual void log(Level level, const Tag& tag, const QString& message) override;

    virtual void logForced(Level level, const Tag& tag, const QString& message) override;

    virtual bool isToBeLogged(Level level, const Tag& tag) override;

    virtual Level defaultLevel() const override;
    virtual void setDefaultLevel(Level level) override;

    virtual LevelFilters levelFilters() const override;
    virtual void setLevelFilters(LevelFilters filters) override;

    virtual void setSettings(const LoggerSettings& settings) override;

    virtual Level maxLevel() const override;

    virtual void setOnLevelChanged(OnLevelChanged onLevelChanged) override;

    virtual std::optional<QString> filePath() const override;

    virtual cf::future<cf::unit> stopArchivingAsync() override;

    virtual void writeLogHeader() override;

    const std::vector<std::unique_ptr<AbstractLogger>>& loggers() const;

private:
    std::vector<std::unique_ptr<AbstractLogger>> m_loggers;
};

} // namespace nx::log
