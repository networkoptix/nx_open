#pragma once

#include <memory>
#include <atomic>

#include <camera/camera_plugin.h>
#include <plugins/plugin_container_api.h>

#include "video_stream.h"
#include "audio_stream.h"

#include "device/video/resolution_data.h"
#include "device/abstract_compression_type_descriptor.h"

namespace nx {
namespace usb_cam {

class CameraManager;

class Camera: public std::enable_shared_from_this<Camera>
{
public:
    Camera(
        CameraManager * cameraManager,
        nxpl::TimeProvider * const timeProvider);

    bool initialize();
    bool isInitialized() const;

    std::shared_ptr<AudioStream> audioStream();
    std::shared_ptr<VideoStream> videoStream();

    std::vector<device::video::ResolutionData> resolutionList() const;

    void setAudioEnabled(bool value);
    bool audioEnabled() const;

    bool ioError() const;

    void setLastError(int errorCode);
    int lastError() const;

    uint64_t millisSinceEpoch() const;
    nxpl::TimeProvider * const timeProvider() const;

    const device::CompressionTypeDescriptorPtr& compressionTypeDescriptor() const;

    CodecParameters defaultVideoParameters() const;

    std::string ffmpegUrl() const;
    
    std::vector<AVCodecID> ffmpegCodecPriorityList();

    const nxcip::CameraInfo& info() const;

    std::string toString() const;

private:
    static const std::vector<nxcip::CompressionType> kVideoCodecPriorityList;

    CameraManager * m_cameraManager;
    nxpl::TimeProvider * const m_timeProvider;
    CodecParameters m_defaultVideoParams;

    std::shared_ptr<AudioStream> m_audioStream;
    std::shared_ptr<VideoStream> m_videoStream;

    bool m_audioEnabled = false;
    std::atomic_int m_lastError = 0;

    device::CompressionTypeDescriptorPtr m_compressionTypeDescriptor;

    bool m_initialized = false;

private:
    CodecParameters getDefaultVideoParameters();
};

} // namespace usb_cams
} // namespace nx