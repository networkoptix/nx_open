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

namespace nx::usb_cam {

class AbstractStreamReader
{
public:
    virtual ~AbstractStreamReader() = default;
    virtual int processPacket(
        const std::shared_ptr<ffmpeg::Packet>& source,
        std::shared_ptr<ffmpeg::Packet>& result) = 0;
    virtual void setFps(float fps) = 0;
    virtual void setResolution(const nxcip::Resolution& resolution) = 0;
    virtual void setBitrate(int bitrate) = 0;
};

class StreamReader: public nxcip::StreamReader
{
public:
    StreamReader(
        nxpt::CommonRefManager* const parentRefManager,
        const std::shared_ptr<Camera>& camera,
        bool forceTranscoding);

    virtual ~StreamReader();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    virtual int addRef() const override;
    virtual int releaseRef() const override;

    virtual int getNextData(nxcip::MediaDataPacket** lpPacket) override;
    virtual void interrupt() override;

    virtual void setFps(float fps);
    virtual void setResolution(const nxcip::Resolution& resolution);
    virtual void setBitrate(int bitrate);

private:
    std::shared_ptr<ffmpeg::Packet> nextPacket();
    void ensureConsumerAdded();
    void removeConsumer();
    bool interrupted();
    int handleNxError();
    bool shouldStop() const;

    std::unique_ptr<ILPMediaPacket> toNxPacket(const ffmpeg::Packet *packet);

private:
    nxpt::CommonRefManager m_refManager;
    std::unique_ptr<AbstractStreamReader> m_streamReader;
    int m_encoderIndex;
    std::shared_ptr<Camera> m_camera;
    std::shared_ptr<BufferedPacketConsumer> m_avConsumer;

    CyclicAllocator m_allocator;

    std::atomic_bool m_videoConsumerAdded = false;
    std::atomic_bool m_audioConsumerAdded = false;
    std::atomic_bool m_interrupted = false;
};

} // namespace usb_cam::nx
