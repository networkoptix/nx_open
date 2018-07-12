#pragma once

#include "abstract_logger.h"

namespace nx {
namespace utils {
namespace log {

class NX_UTILS_API Logger:
    public AbstractLogger
{
public:
    Logger(const std::set<Tag>& tags);

    /** Initializes log with minimal level and writers, no writers means std out and err. */
    Logger(
        Level defaultLevel = Level::none,
        std::unique_ptr<AbstractWriter> writer = nullptr,
        OnLevelChanged onLevelChanged = nullptr);

    virtual std::set<Tag> tags() const override;

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

    /** @return Maximum possible log level, according to the current settings. */
    virtual Level maxLevel() const override;

    virtual void setWriter(std::unique_ptr<AbstractWriter> writer) override;

    virtual void setOnLevelChanged(OnLevelChanged onLevelChanged) override;

    virtual std::optional<QString> filePath() const override;

private:
    void handleLevelChange(QnMutexLockerBase* lock) const;

private:
    mutable QnMutex m_mutex;
    const std::set<Tag> m_tags;
    OnLevelChanged m_onLevelChanged;
    Level m_defaultLevel = Level::none;
    std::vector<std::unique_ptr<AbstractWriter>> m_writers;
    LevelFilters m_levelFilters;
};

} // namespace log
} // namespace utils
} // namespace nx
