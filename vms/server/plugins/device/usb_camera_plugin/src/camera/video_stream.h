#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>

#include "ffmpeg/input_format.h"
#include "ffmpeg/packet.h"
#include "codec_parameters.h"
#include "device/video/resolution_data.h"
#include "device/abstract_compression_type_descriptor.h"

namespace nxpl { class TimeProvider; }

namespace nx::usb_cam {

class Camera;

class VideoStream
{
public:
    VideoStream(const std::string& url, nxpl::TimeProvider* timeProvider);
    virtual ~VideoStream();

    int initialize();
    int nextPacket(std::shared_ptr<ffmpeg::Packet>& result);
    void uninitializeInput();

    void setFps(float fps);
    void setResolution(const nxcip::Resolution& resolution);
    void setBitrate(int bitrate);
    void updateUrl(const std::string& url);
    bool isVideoCompressed();
    CodecParameters codecParameters();
    int getMaxBitrate();

    /**
     * Queries hardware for the camera name to determine if the camera is plugged in.
     */
    bool pluggedIn() const;
    std::vector<device::video::ResolutionData> resolutionList() const;
    AVCodecParameters* getCodecParameters();

private:
    std::string m_url;
    std::weak_ptr<Camera> m_camera;
    CodecParameters m_codecParams;
    nxpl::TimeProvider* const m_timeProvider;
    std::atomic_bool m_needReinitialization = true;
    std::unique_ptr<ffmpeg::InputFormat> m_inputFormat;
    device::CompressionTypeDescriptorPtr m_compressionTypeDescriptor;
    mutable std::mutex m_mutex;

private:
    int ensureInitialized();
    /**
     * Get the url of the video stream, modified appropriately based on platform.
     */
    std::string ffmpegUrlPlatformDependent() const;
    int initializeInput();
    void setInputFormatOptions(std::unique_ptr<ffmpeg::InputFormat>& inputFormat);
    CodecParameters findClosestHardwareConfiguration(const CodecParameters& params) const;
    void setCodecParameters(const CodecParameters& codecParams);
    CodecParameters getDefaultVideoParameters();
};

} // namespace nx::usb_cam
