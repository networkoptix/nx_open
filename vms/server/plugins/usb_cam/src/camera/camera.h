#pragma once

#include <memory>
#include <atomic>

#include <camera/camera_plugin.h>
#include <plugins/plugin_container_api.h>

#include "video_stream.h"
#include "audio_stream.h"

#include "../device/abstract_compression_type_descriptor.h"
#include "../device/device_data.h"

namespace nx {
namespace usb_cam {

class Camera: public std::enable_shared_from_this<Camera>
{
public:
    Camera(
        nxpl::TimeProvider * const timeProvider,
        const nxcip::CameraInfo& info);
    virtual ~Camera() = default;

    void initialize();

    std::shared_ptr<AudioStream> audioStream();
    std::shared_ptr<VideoStream> videoStream();

    std::vector<device::ResolutionData> resolutionList() const;

    void setAudioEnabled(bool value);
    bool audioEnabled() const;

    bool ioError() const;

    void setLastError(int errorCode);
    int lastError() const;

    const nxcip::CameraInfo& info() const;
    void updateCameraInfo(const nxcip::CameraInfo& info);

    uint64_t millisSinceEpoch() const;
    nxpl::TimeProvider * const timeProvider() const;

    const device::CompressionTypeDescriptorPtr& compressionTypeDescriptor() const;

    /**
     * Return the url used by ffmpeg to open the video stream
     */
    std::string url() const;
    CodecParameters defaultVideoParameters() const;

    std::vector<AVCodecID> ffmpegCodecPriorityList();

private:
    static const std::vector<nxcip::CompressionType> kVideoCodecPriorityList;

    nxpl::TimeProvider * const m_timeProvider;
    nxcip::CameraInfo m_info;
    CodecParameters m_defaultVideoParams;

    std::shared_ptr<AudioStream> m_audioStream;
    std::shared_ptr<VideoStream> m_videoStream;

    bool m_audioEnabled;
    std::atomic_int m_lastError;

    device::CompressionTypeDescriptorPtr m_compressionTypeDescriptor;

private:
    CodecParameters getDefaultVideoParameters();
};

} // namespace usb_cams
} // namespace nx