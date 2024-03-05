// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <nx/utils/lockable.h>

#include "abstract_logger.h"
#include "log_settings.h"

namespace nx::log {

class NX_UTILS_API Logger:
    public AbstractLogger
{
public:
    /** Initializes log with minimal level and writers, no writers means std out and err. */
    Logger(
        std::set<Filter> filters,
        Level defaultLevel = Level::none,
        std::unique_ptr<AbstractWriter> writer = nullptr);

    virtual std::set<Filter> filters() const override;

    /** Writes message to every writer if it is to be logged. */
    virtual void log(Level level, const Tag& tag, const QString& message) override;

    /** Writes message to every writer unconditionally. */
    virtual void logForced(Level level, const Tag& tag, const QString& message) override;

    /** Indicates whether this logger is going to log such a message. */
    virtual bool isToBeLogged(Level level, const Tag& tag = {}) override;

    /** Makes this logger log all messages with level <= defaultLevel. */
    virtual Level defaultLevel() const override;
    virtual void setDefaultLevel(Level level) override;

    /** Custom levels for messages which tag starting with one of the filters. */
    virtual LevelFilters levelFilters() const override;
    virtual void setLevelFilters(LevelFilters filters) override;

    virtual void setSettings(const LoggerSettings& loggerSettings) override;

    /** @return Maximum possible log level, according to the current settings. */
    virtual Level maxLevel() const override;

    virtual void setOnLevelChanged(OnLevelChanged onLevelChanged) override;

    virtual std::optional<QString> filePath() const override;

    virtual cf::future<cf::unit> stopArchivingAsync() override;

    virtual void writeLogHeader() override;

    void setApplicationName(const QString& applicationName);
    void setBinaryPath(const QString& binaryPath);
    void setWriter(std::unique_ptr<AbstractWriter> writer);

private:
    void handleLevelChange(nx::Locker<nx::Mutex>* lock) const;

private:
    class OneSecondCache
    {
    public:
        QString now();

    private:
        std::chrono::seconds m_lastTimestamp;
        QString m_cachedDateTime;
    };

    mutable nx::Mutex m_mutex;
    const std::set<Filter> m_hardFilters;
    Level m_defaultLevel = Level::none;
    OnLevelChanged m_onLevelChanged;
    LoggerSettings m_settings;
    QString m_applicationName;
    QString m_binaryPath;
    Lockable<OneSecondCache> m_dateTimeCache;
    std::unique_ptr<AbstractWriter> m_writer;
    LevelFilters m_levelFilters;
};

} // namespace nx::log
