#pragma once

#include <memory>

#include <QtCore/QFile>

#include <nx/utils/thread/mutex.h>

namespace nx::vms::testcamera {

class Logger;

/**
 * Logs frame data to a dedicated file if enabled by .ini.
 */
class FrameLogger final
{
public:
    FrameLogger();
    ~FrameLogger();

    /**
     * Writes the message to the log file, adding the prefix and the context from the logger.
     * @param logger Used to obtain log prefix and context.
     */
    void logFrameIfNeeded(const QString& message, const Logger* logger) const;

private:
    mutable nx::utils::Mutex m_logFramesFileMutex;
    std::unique_ptr<QFile> m_logFramesFile; //< Null if frame logging is disabled.
};

} // namespace nx::vms::testcamera
