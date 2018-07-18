#pragma once

#include <memory>

#include <QtCore/QUrl>

extern "C" {
#include <libavcodec/avcodec.h>
} // extern C

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_api.h>
#include <plugins/plugin_container_api.h>
#include <utils/memory/cyclic_allocator.h>

#include "ilp_video_packet.h"
#include "codec_context.h"
#include "ffmpeg/forward_declarations.h"
#include "ffmpeg/stream_reader.h"
#include "ffmpeg/buffered_stream_consumer.h"

namespace nx {
namespace rpi_cam2 {
    
//!Transfers or transcodes packets from USB webcameras and streams them
class StreamReader
:
    public nxcip::StreamReader
{
public:
    StreamReader(
        int encoderIndex,
        nxpt::CommonRefManager* const parentRefManager,
        nxpl::TimeProvider *const timeProvider,
        const nxcip::CameraInfo& cameraInfo,
        const CodecContext& codecContext,
        const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~StreamReader();

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

    virtual int getNextData( nxcip::MediaDataPacket** packet ) = 0;
    virtual void interrupt() override;

    virtual void setFps(int fps);
    virtual void setResolution(const nxcip::Resolution& resolution);
    virtual void setBitrate(int bitrate);

    void updateCameraInfo( const nxcip::CameraInfo& info );

protected:
    int m_encoderIndex;
    nxpt::CommonRefManager m_refManager;
    nxpl::TimeProvider* const m_timeProvider;
    CyclicAllocator m_allocator;
    nxcip::CameraInfo m_info;

    bool m_interrupted = false;

    CodecContext m_codecContext;
    std::shared_ptr<ffmpeg::StreamReader> m_ffmpegStreamReader;
    std::shared_ptr<ffmpeg::BufferedStreamConsumer> m_consumer;
    int m_lastFfmpegError = 0;

protected:
    std::unique_ptr<ILPVideoPacket> toNxPacket(AVPacket *packet, AVCodecID codecID, uint64_t time);
    std::shared_ptr<ffmpeg::Packet> nextPacket();
    void maybeDropPackets();
};

} // namespace rpi_cam2
} // namespace nx
