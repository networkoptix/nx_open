#pragma once

#include <memory>
#include <mutex>

#include <camera/camera_plugin.h>
#include <plugins/plugin_container_api.h>

#include "video_stream.h"
#include "audio_stream.h"

#include "../device/abstract_compression_type_descriptor.h"
#include "../device/device_data.h"

namespace nx {
namespace usb_cam {

class Camera : public std::enable_shared_from_this<Camera>
{
public:
    Camera(
        nxpl::TimeProvider * const timeProvider,
        const nxcip::CameraInfo& info);

    std::shared_ptr<AudioStream> audioStream();
    std::shared_ptr<VideoStream> videoStream();

    std::vector<device::ResolutionData> getResolutionList() const;

    void setAudioEnabled(bool value);
    bool audioEnabled() const;

    void setLastError(int errorCode);
    int lastError() const;

    const nxcip::CameraInfo& info() const;
    void updateCameraInfo(const nxcip::CameraInfo& info);

    uint64_t millisSinceEpoch() const;
    nxpl::TimeProvider * const timeProvider() const;

    device::CompressionTypeDescriptorConstPtr compressionTypeDescriptor() const;

    std::string url() const;
    CodecParameters codecParameters() const;

private:
    nxpl::TimeProvider * const m_timeProvider;
    nxcip::CameraInfo m_info;
    CodecParameters m_videoCodecParams;

    mutable std::mutex m_mutex;
    std::shared_ptr<AudioStream> m_audioStream;
    std::shared_ptr<VideoStream> m_videoStream;

    bool m_audioEnabled;
    int m_lastError;

    device::CompressionTypeDescriptorPtr m_compressionTypeDescriptor;

private:
    void assignDefaultParams(const device::CompressionTypeDescriptorPtr& descriptor);
};

} // namespace usb_cams
} // namespace nx