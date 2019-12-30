#include "camera.h"

#include <nx/kit/utils.h>
#include <nx/network/socket.h>
#include <nx/utils/random.h>
#include <nx/streaming/video_data_packet.h>
#include <core/resource/avi/avi_archive_delegate.h>
#include <utils/common/sleep.h>
#include <nx/network/abstract_socket.h>

#include <nx/vms/testcamera/test_camera_ini.h>

#include "logger.h"
#include "file_cache.h"
#include "file_streamer.h"

using namespace std::chrono;
using namespace std::literals::chrono_literals;

namespace nx::vms::testcamera {

static constexpr unsigned int kSendTimeoutMs = 1000;

static nx::utils::MacAddress makeMacAddress(int cameraNumber)
{
    QString macAddressString = ini().macAddressPrefix;
    for (int i = 3; i >= 0; --i)
        macAddressString += QString().sprintf("-%02x", (cameraNumber >> (i * 8)) & 0xFF);

    nx::utils::MacAddress macAddress(macAddressString);

    NX_ASSERT(!macAddress.isNull(),
        "Invalid camera MAC address string: %1" + nx::kit::utils::toString(macAddressString));

    return macAddress;
}

class FrameScheduler
{
public:
    FrameScheduler(int fps): m_fps(fps)
    {
        m_frameIntervalTimer.restart();
    }

    void sleepAfterFrame()
    {
        m_streamingTimeMs += 1000.0 / m_fps;
        const int waitingTimeMs = (int) m_streamingTimeMs - m_frameIntervalTimer.elapsed();
        if (waitingTimeMs > 0)
            QnSleep::msleep(waitingTimeMs);
    }

private:
    const int m_fps;
    double m_streamingTimeMs = 0;
    QTime m_frameIntervalTimer;
};

Camera::Camera(
    const FrameLogger* frameLogger,
    const FileCache* fileCache,
    int number,
    const CameraOptions& cameraOptions,
    const QStringList& primaryFileNames,
    const QStringList& secondaryFileNames)
    :
    m_frameLogger(frameLogger),
    m_fileCache(fileCache),
    m_number(number),
    m_macAddress(makeMacAddress(number)),
    m_cameraOptions(cameraOptions),
    m_primaryFileNames(primaryFileNames),
    m_secondaryFileNames(secondaryFileNames),
    m_logger(new Logger(lm("Camera(%1)").args(m_macAddress)))
{
    m_checkTimer.restart();
}

Camera::~Camera()
{
}

bool Camera::performStreamingFile(
    const std::vector<std::shared_ptr<const QnCompressedVideoData>>& frames,
    FrameScheduler* frameScheduler,
    FileStreamer* fileStreamer,
    Logger* logger)
{
    for (int i = 0; i < (int) frames.size(); ++i)
    {
        const auto frameLoggerContext = logger->pushContext(
            lm("frame #%1 with PTS %2").args(i, us(fileStreamer->framePts(frames[i].get()))));

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
            if (ini().stopStreamingOnErrors)
            {
                NX_LOGGER_ERROR(logger,
                    "Frame sending failed due to the above error; stop streaming.");
                return false;
            }
        }

        frameScheduler->sleepAfterFrame();
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
    if (m_cameraOptions.unloopPts && !NX_ASSERT(filenames.size() == 1))
        return;

    // Clone the logger because this function can be called reenterably for various Servers.
    const auto logger = m_logger->cloneForContext(
        lm("Server %1, %2 stream").args(socket->getForeignAddress(), streamIndex));

    NX_LOGGER_INFO(logger, "Start streaming at %1 FPS.", fps);

    socket->setSendTimeout(kSendTimeoutMs);

    const std::unique_ptr<FileStreamer::PtsUnloopingContext> ptsUnloopingContext{
        m_cameraOptions.unloopPts ? new FileStreamer::PtsUnloopingContext : nullptr};

    FrameScheduler frameScheduler(fps);

    for (;;) //< Stream all files in the list infinitely, unless an error occurs.
    {
        for (const QString filename: filenames)
        {
            const auto& file = m_fileCache->getFile(filename);

            const auto fileLoggerContext = logger->pushContext(lm("file #%1%2").args(file.index,
                ptsUnloopingContext ? lm(", loop #%1").args(ptsUnloopingContext->loopIndex) : ""));

            FileStreamer fileStreamer(
                logger.get(),
                m_frameLogger,
                m_cameraOptions,
                socket,
                streamIndex,
                file.channelCount,
                ptsUnloopingContext.get());

            if (!performStreamingFile(file.frames, &frameScheduler, &fileStreamer, logger.get()))
                return;
        }
    }
}

void Camera::makeOfflineFloodIfNeeded()
{
    NX_MUTEX_LOCKER lock(&m_offlineMutex);

    if (m_cameraOptions.offlineFreq == 0 || m_checkTimer.elapsed() < 1000)
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

    if (m_isEnabled && nx::utils::random::number(0, 99) < m_cameraOptions.offlineFreq)
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
