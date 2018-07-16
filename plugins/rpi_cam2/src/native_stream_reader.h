#pragma once

#include "stream_reader.h"

namespace nx {
namespace rpi_cam2 {

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
        const std::shared_ptr<nx::ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~NativeStreamReader();

    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;

private:
    bool m_initialized;
};

} // namespace rpi_cam2
} // namespace nx
