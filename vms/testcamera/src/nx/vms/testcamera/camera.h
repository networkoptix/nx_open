#pragma once

#include <atomic>

#include <nx/streaming/media_data_packet.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/mac_address.h>

#include <nx/vms/api/types/motion_types.h> //< For StreamIndex.
using nx::vms::api::StreamIndex;

#include "camera_options.h"

namespace nx::network { class AbstractStreamSocket; }
class QnCompressedVideoData; //< private

namespace nx::vms::testcamera {

class FrameLogger;
class FileCache;
class Logger; //< private
class FileStreamer; //< private
class FrameScheduler; //< private

/**
 * Streams the files loaded into FileCache in an infinite loop. Exits only on error.
 */
class Camera final
{
public:
    Camera(
        const FrameLogger* frameLogger,
        const FileCache* fileCache,
        int number,
        const CameraOptions& cameraOptions,
        const QStringList& primaryFileNames,
        const QStringList& secondaryFileNames);

    ~Camera();

    int number() const { return m_number; }
    nx::utils::MacAddress macAddress() const { return m_macAddress; }

    /**
     * Streams the files for the specified stream in an infinite loop. Exits only on error.
     */
    void performStreaming(
        nx::network::AbstractStreamSocket* socket, StreamIndex streamIndex, int fps);

    bool isEnabled();

private:
    bool performStreamingFile(
        const std::vector<std::shared_ptr<const QnCompressedVideoData>>& frames,
        FrameScheduler* frameScheduler,
        FileStreamer* fileStreamer,
        Logger* logger);

    void makeOfflineFloodIfNeeded();

private:
    const FrameLogger* const m_frameLogger;
    const FileCache* const m_fileCache;
    const int m_number;
    const nx::utils::MacAddress m_macAddress;
    const CameraOptions m_cameraOptions;
    const QStringList m_primaryFileNames;
    const QStringList m_secondaryFileNames;

    const std::unique_ptr<Logger> m_logger;

    // State for supporting 'offline' parameter.
    mutable nx::utils::Mutex m_offlineMutex;
    std::atomic<bool> m_isEnabled = true;
    QTime m_offlineTimer;
    QTime m_checkTimer;
    int m_offlineDurationMs = 0;
};

} // namespace nx::vms::testcamera
