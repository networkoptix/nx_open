#include "frame_logger.h"

#include <QtCore/QString>

#include <core/resource/test_camera_ini.h>

#include "logger.h"

namespace nx::vms::testcamera {

static QString currentDateTime()
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
}

FrameLogger::FrameLogger()
{
    const QString logFramesFilename = testCameraIni().logFramesFile;
    if (logFramesFilename.isEmpty())
        return;

    const auto logger = std::make_unique<Logger>("FrameLogger");

    // Open frame log file on first need.
    if (!m_logFramesFile)
    {
        m_logFramesFile = std::make_unique<QFile>(logFramesFilename);

        if (!m_logFramesFile->open(QIODevice::WriteOnly))
        {
            NX_LOGGER_ERROR(logger, "Unable to open frames logging file: %1", logFramesFilename);
            m_logFramesFile.reset();
            return;
        }

        NX_LOGGER_INFO(logger, "INI: Logging frames to file: %1", logFramesFilename);
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

    if (m_logFramesFile->write(line.toUtf8()) <= 0)
        NX_LOGGER_ERROR(logger, "Unable to log frame to file: %1", m_logFramesFile->fileName());
}

} // namespace nx::vms::testcamera
