#pragma once

#include "stream_reader.h"

namespace nx {
namespace webcam_plugin {

namespace ffmpeg { class StreamReader; }

//!Transfers or transcodes packets from USB webcameras and streams them
class NativeStreamReader
:
    public StreamReader
{
public:
    NativeStreamReader(
        nxpt::CommonRefManager* const parentRefManager,
        nxpl::TimeProvider *const timeProvider,
        const nxcip::CameraInfo& cameraInfo,
        const CodecContext& codecContext,
        const std::weak_ptr<ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~NativeStreamReader();

    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;

    virtual void setFps(int fps) override;
    virtual void setResolution(const nxcip::Resolution& resolution) override;
    virtual void setBitrate(int bitrate) override;
    void updateCameraInfo( const nxcip::CameraInfo& info );
};

} // namespace webcam_plugin
} // namespace nx
