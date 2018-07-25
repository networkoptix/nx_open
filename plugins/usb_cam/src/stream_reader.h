#pragma once

#include <memory>

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
#include "ffmpeg/stream_reader.h"
#include "ffmpeg/codec_parameters.h"
#include "ffmpeg/buffered_stream_consumer.h"

namespace nx {
namespace usb_cam {

class InternalStreamReader;

//! Transfers or transcodes packets from USB webcameras and streams them
class StreamReader : public nxcip::StreamReader{
public:
    StreamReader(
        int encoderIndex,
        nxpl::TimeProvider *const timeProvider,
        const ffmpeg::CodecParameters& codecParams,
        const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader,
        nxpt::CommonRefManager* const parentRefManager);

    StreamReader(
        std::unique_ptr<InternalStreamReader>& streamReader,
        nxpl::TimeProvider *const timeProvider,
        nxpt::CommonRefManager* const parentRefManager);

    virtual ~StreamReader();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

    virtual int getNextData(nxcip::MediaDataPacket** lpPacket) override;
    virtual void interrupt() override;

    virtual void setFps(int fps);
    virtual void setResolution(const nxcip::Resolution& resolution);
    virtual void setBitrate(int bitrate);

    int lastFfmpegError() const;

private:
    nxpl::TimeProvider* const m_timeProvider;
    nxpt::CommonRefManager m_refManager;
    std::unique_ptr<InternalStreamReader> m_streamReader;
};

class InternalStreamReader    
{
public:
    InternalStreamReader(
        int encoderIndex,
        nxpl::TimeProvider *const timeProvider,
        const ffmpeg::CodecParameters& codecParams,
        const std::shared_ptr<ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~InternalStreamReader();

    virtual int getNextData(nxcip::MediaDataPacket** packet) = 0;
    virtual void interrupt();

    virtual void setFps(int fps);
    virtual void setResolution(const nxcip::Resolution& resolution);
    virtual void setBitrate(int bitrate);

    int lastFfmpegError() const;

protected:
    int m_encoderIndex;
    nxpl::TimeProvider* const m_timeProvider;
    CyclicAllocator m_allocator;

    ffmpeg::CodecParameters m_codecParams;
    std::shared_ptr<ffmpeg::StreamReader> m_ffmpegStreamReader;
    std::shared_ptr<ffmpeg::BufferedStreamConsumer> m_consumer;

    bool m_interrupted;
    int m_lastFfmpegError;

protected:
    std::unique_ptr<ILPVideoPacket> toNxPacket(AVPacket *packet, AVCodecID codecID, uint64_t timeUsec, bool forceKeyPacket);
    std::shared_ptr<ffmpeg::Packet> nextPacket();
    void maybeDropPackets();
};

} // namespace usb_cam
} // namespace nx
