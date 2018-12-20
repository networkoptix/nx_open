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
#include "camera/camera.h"
#include "camera/codec_parameters.h"
#include "camera/buffered_stream_consumer.h"

namespace nx {
namespace usb_cam {

class StreamReaderPrivate;

class StreamReader: public nxcip::StreamReader
{
public:
    StreamReader(
        nxpt::CommonRefManager* const parentRefManager,
        int encoderIndex,
        const CodecParameters& codecParams,
        const std::shared_ptr<Camera>& camera,
        bool forceTranscoding);

    virtual ~StreamReader() = default;

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
        const CodecParameters& codecParams,
        const std::shared_ptr<Camera>& camera);

    virtual ~StreamReaderPrivate();

    virtual int getNextData(nxcip::MediaDataPacket** packet) = 0;
    virtual void interrupt();

    virtual void setFps(float fps);
    virtual void setResolution(const nxcip::Resolution& resolution);
    virtual void setBitrate(int bitrate);

    virtual void ensureConsumerAdded();

protected:
    static constexpr int kMsecInSec = 1000;
    static constexpr std::chrono::milliseconds kWaitTimeout = std::chrono::milliseconds(1000);

    int m_encoderIndex;
    CodecParameters m_codecParams;
    std::shared_ptr<Camera> m_camera;
    std::shared_ptr<BufferedPacketConsumer> m_avConsumer;

    CyclicAllocator m_allocator;

    std::atomic_bool m_videoConsumerAdded = false;
    std::atomic_bool m_audioConsumerAdded = false;
    std::atomic_bool m_interrupted = false;

protected:
    std::unique_ptr<ILPMediaPacket> toNxPacket(const ffmpeg::Packet *packet);
    void removeAudioConsumer();
    virtual void removeVideoConsumer() = 0;
    void removeConsumer();
};

} // namespace usb_cam
} // namespace nx
