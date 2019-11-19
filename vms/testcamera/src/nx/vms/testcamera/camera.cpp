#include "camera.h"

#include <nx/kit/debug.h>
#include <nx/network/socket.h>
#include <nx/utils/random.h>
#include <nx/streaming/video_data_packet.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <utils/common/sleep.h>
#include <core/resource/test_camera_ini.h>
#include <nx/network/abstract_socket.h>

#include "logger.h"
#include "file_cache.h"
#include "file_streamer.h"

using namespace std::chrono;
using namespace std::literals::chrono_literals;

namespace nx::vms::testcamera {

static constexpr unsigned int kSendTimeoutMs = 1000;

static QByteArray buildMac(int cameraNumber)
{
    const auto numberBigEndian = qToBigEndian((int32_t) cameraNumber);

    QByteArray mac = testCameraIni().macPrefix;
    QByteArray last = QByteArray((const char*) &numberBigEndian, sizeof(numberBigEndian)).toHex();
    while (!last.isEmpty())
    {
        mac += '-';
        mac += last.left(2);
        last = last.mid(2);
    }

    return mac;
}

Camera::Camera(
    const FrameLogger* frameLogger,
    const FileCache* fileCache,
    int number,
    const CameraOptions& testCameraOptions,
    const QStringList& primaryFileNames,
    const QStringList& secondaryFileNames)
    :
    m_frameLogger(frameLogger),
    m_fileCache(fileCache),
    m_number(number),
    m_mac(buildMac(number)),
    m_options(testCameraOptions),
    m_primaryFileNames(primaryFileNames),
    m_secondaryFileNames(secondaryFileNames),
    m_logger(new Logger(lm("Camera(%1)").args(m_mac)))
{
    m_checkTimer.restart();
}

Camera::~Camera()
{
}

bool Camera::performStreamingFile(
    const QList<QnConstCompressedVideoDataPtr>& frames,
    int fps,
    FileStreamer* fileStreamer,
    Logger* logger)
{
    double streamingTimeMs = 0;
    QTime frameIntervalTimer;
    frameIntervalTimer.restart();

    for (int i = 0; i < frames.size(); ++i)
    {
        const auto frameLoggerContext = logger->pushContext(
            lm("frame #%1 with pts %2").args(i, us(fileStreamer->framePts(frames[i].get()))));

        makeOfflineFloodIfNeeded();
        if (!m_isEnabled)
        {
            QnSleep::msleep(100);
            NX_LOGGER_INFO(logger,
                "The camera was disabled as per 'offline' parameter; stop streaming.");
            return false;
        }

        if (!fileStreamer->streamFrame(frames[i].get(), i))
        {
            if (testCameraIni().stopStreamingOnSocketErrors)
            {
                NX_LOGGER_ERROR(logger,
                    "Frame sending failed due to the above error; stop streaming.");
                return false;
            }
        }

        streamingTimeMs += 1000.0 / fps;
        const int waitingTime = (int) streamingTimeMs - frameIntervalTimer.elapsed();
        if (waitingTime > 0)
            QnSleep::msleep(waitingTime);
    }
    return true;
}

void Camera::performStreaming(
    nx::network::AbstractStreamSocket* socket, StreamIndex streamIndex, int fps)
{
    const QStringList& filenames =
        (streamIndex == StreamIndex::secondary) ? m_secondaryFileNames : m_primaryFileNames;
    if (!NX_ASSERT(!filenames.isEmpty()))
        return;
    if (m_options.unloopPts && !NX_ASSERT(filenames.size() == 1))
        return;

    // Clone the logger because this function can be called reenterably for various Servers.
    const auto logger = m_logger->cloneForContext(
        lm("Server %1, %2 stream").args(socket->getForeignAddress(), streamIndex));

    NX_LOGGER_INFO(logger, "Start streaming at %1 FPS.", fps);

    socket->setSendTimeout(kSendTimeoutMs);

    const std::unique_ptr<FileStreamer::PtsUnloopingContext> ptsUnloopingContext{
        m_options.unloopPts ? new FileStreamer::PtsUnloopingContext : nullptr};

    for (;;) //< Stream all files in the list infinitely, unless an error occurs.
    {
        for (const QString filename: filenames)
        {
            const auto& file = m_fileCache->getFile(filename);
            if (!NX_ASSERT(!file.frames.empty()))
                return;

            const auto fileLoggerContext = logger->pushContext(lm("file #%1%2").args(file.index,
                ptsUnloopingContext ? lm(", loop #%1").args(ptsUnloopingContext->loopIndex) : ""));

            const auto fileStreamer = std::make_unique<FileStreamer>(
                logger.get(), m_frameLogger, m_options, socket, streamIndex, filename,
                ptsUnloopingContext.get());

            if (!performStreamingFile(file.frames, fps, fileStreamer.get(), logger.get()))
                return;
        }
    }
}

void Camera::makeOfflineFloodIfNeeded()
{
    NX_MUTEX_LOCKER lock(&m_offlineMutex);

    if (m_options.offlineFreq == 0 || m_checkTimer.elapsed() < 1000)
        return;

    m_checkTimer.restart();

    if (!m_isEnabled)
    {
        if (m_offlineTimer.elapsed() > m_offlineDurationMs)
        {
            NX_LOGGER_INFO(m_logger, "Enabling the camera as per 'offline' parameter.");
            m_isEnabled = true;
            return;
        }
    }

    if (m_isEnabled && (nx::utils::random::number(0, 99) < m_options.offlineFreq))
    {
        m_isEnabled = false;
        m_offlineTimer.restart();
        m_offlineDurationMs = nx::utils::random::number(2000, 4000);
        NX_LOGGER_INFO(m_logger, "Disabling the camera for %1 as per 'offline' parameter.",
            m_offlineDurationMs);
    }
}

bool Camera::isEnabled()
{
    makeOfflineFloodIfNeeded();
    return m_isEnabled;
}

} // namespace nx::vms::testcamera
