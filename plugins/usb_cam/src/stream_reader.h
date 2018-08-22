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

#include "ilp_media_packet.h"
#include "camera.h"
#include "ffmpeg/codec_parameters.h"
#include "ffmpeg/buffered_stream_consumer.h"

namespace nx {
namespace usb_cam {

class StreamReaderPrivate;

//! Transfers or transcodes packets from USB webcameras and streams them
class StreamReader : public nxcip::StreamReader 
{
public:
    StreamReader(
        nxpt::CommonRefManager* const parentRefManager,
        int encoderIndex,
        const ffmpeg::CodecParameters& codecParams,
        const std::shared_ptr<Camera>& camera);

    StreamReader(
        nxpt::CommonRefManager* const parentRefManager,
        std::unique_ptr<StreamReaderPrivate> streamReader);

    virtual ~StreamReader();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

    virtual int getNextData(nxcip::MediaDataPacket** lpPacket) override;
    virtual void interrupt() override;

    virtual void setFps(float fps);
    virtual void setResolution(const nxcip::Resolution& resolution);
    virtual void setBitrate(int bitrate);

private:
    nxpt::CommonRefManager m_refManager;
    std::unique_ptr<StreamReaderPrivate> m_streamReader;
};

class StreamReaderPrivate
{
public:
    StreamReaderPrivate(
        int encoderIndex,
        const ffmpeg::CodecParameters& codecParams,
        const std::shared_ptr<Camera>& camera);

    virtual ~StreamReaderPrivate();

    virtual int getNextData(nxcip::MediaDataPacket** packet) = 0;
    virtual void interrupt();

    virtual void setFps(float fps);
    virtual void setResolution(const nxcip::Resolution& resolution);
    virtual void setBitrate(int bitrate);


    virtual void ensureConsumerAdded();

protected:
    CyclicAllocator m_allocator;

    int m_encoderIndex;
    ffmpeg::CodecParameters m_codecParams;
    std::shared_ptr<Camera> m_camera;

    std::shared_ptr<ffmpeg::BufferedPacketConsumer> m_audioConsumer;

    bool m_consumerAdded;
    bool m_interrupted;

protected:
    std::unique_ptr<ILPMediaPacket> toNxPacket(
        AVPacket *packet,
        AVCodecID codecID,
        nxcip::DataPacketType mediaType,
        uint64_t timeUsec,
        bool forceKeyPacket);
};

} // namespace usb_cam
} // namespace nx
