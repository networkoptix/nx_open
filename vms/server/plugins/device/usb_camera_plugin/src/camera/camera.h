#pragma once

#include <memory>
#include <atomic>

#include <camera/camera_plugin.h>
#include <plugins/plugin_container_api.h>

#include "video_stream.h"
#include "audio_stream.h"

#include "device/video/resolution_data.h"
#include "device/abstract_compression_type_descriptor.h"

namespace nx::usb_cam {

class Camera: public std::enable_shared_from_this<Camera>
{
public:
    Camera(
        const std::string& url,
        const nxcip::CameraInfo& cameraInfo,
        nxpl::TimeProvider* const timeProvider);
    virtual ~Camera() = default;

    virtual void setCredentials( const char* username, const char* password );

    bool initialize();
    bool isInitialized() const;

    std::shared_ptr<AudioStream> audioStream();
    std::shared_ptr<VideoStream> videoStream();

    std::vector<device::video::ResolutionData> resolutionList() const;

    bool hasAudio() const;
    void setAudioEnabled(bool value);
    bool audioEnabled() const;
    void setUrl(const std::string& url);

    bool ioError() const;

    void setLastError(int errorCode);
    int lastError() const;

    uint64_t millisSinceEpoch() const;
    nxpl::TimeProvider* const timeProvider() const;

    const device::CompressionTypeDescriptorPtr& compressionTypeDescriptor() const;
    CodecParameters defaultVideoParameters() const;
    std::string ffmpegUrl() const;
    const nxcip::CameraInfo& info() const;
    std::string toString() const;

private:
    static const std::vector<nxcip::CompressionType> kVideoCodecPriorityList;

    std::string m_url;
    nxcip::CameraInfo m_cameraInfo;
    nxpl::TimeProvider* const m_timeProvider;
    CodecParameters m_defaultVideoParams;
    mutable std::mutex m_mutex;

    std::shared_ptr<AudioStream> m_audioStream;
    std::shared_ptr<VideoStream> m_videoStream;

    bool m_audioEnabled = false;
    std::atomic_int m_lastError = 0;

    device::CompressionTypeDescriptorPtr m_compressionTypeDescriptor;

    bool m_initialized = false;

private:
    CodecParameters getDefaultVideoParameters();
};

} // namespace nx::usb_cams
