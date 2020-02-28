#include "frame_logger.h"

#include <QtCore/QDir>
#include <QtCore/QString>
#include <QtCore/QFileInfo>

#include <nx/vms/testcamera/test_camera_ini.h>

#include "logger.h"

namespace nx::vms::testcamera {

static QString currentDateTime()
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
}

FrameLogger::FrameLogger(const std::optional<QString>& frameLogFilename)
{
    if (frameLogFilename)
    {
        const auto logger = std::make_unique<Logger>("FrameLogger");

        m_logFramesFile = std::make_unique<QFile>(*frameLogFilename);

        if (!m_logFramesFile->open(QIODevice::WriteOnly))
        {
            NX_LOGGER_ERROR(logger, "Unable to open frames logging file: %1", *frameLogFilename);
            m_logFramesFile.reset();
            return;
        }

        NX_LOGGER_INFO(logger, "Logging frames to file: %1",
            QFileInfo(*m_logFramesFile).absoluteFilePath());
    }
}

FrameLogger::~FrameLogger()
{
}

void FrameLogger::logFrameIfNeeded(const QString& message, const Logger* logger) const
{
    QnMutexLocker lock(&m_logFramesFileMutex);
    if (!m_logFramesFile)
        return;

    const QString line = currentDateTime() + " " + logger->tag() + ": "
        + message + logger->context() + "\n";

    NX_LOGGER_VERBOSE(logger, message);

    if (m_logFramesFile->write(line.toUtf8()) <= 0)
    {
        NX_LOGGER_ERROR(logger, "Unable to log frame to file: %1",
            QFileInfo(*m_logFramesFile).absoluteFilePath());
    }
}

} // namespace nx::vms::testcamera
