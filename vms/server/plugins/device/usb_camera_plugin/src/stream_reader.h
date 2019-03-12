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
#include "transcode_stream_reader.h"

namespace nx::usb_cam {

class StreamReader: public nxcip::StreamReader
{
public:
    StreamReader(
        nxpt::CommonRefManager* const parentRefManager,
        const std::shared_ptr<Camera>& camera,
        bool isPrimaryStream);

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;
    virtual int addRef() const override;
    virtual int releaseRef() const override;

    virtual int getNextData(nxcip::MediaDataPacket** lpPacket) override;
    virtual void interrupt() override;

    void setFps(float fps);
    void setResolution(const nxcip::Resolution& resolution);
    void setBitrate(int bitrate);

private:
    int nextPacket(std::shared_ptr<ffmpeg::Packet>& packet);
    std::unique_ptr<ILPMediaPacket> toNxPacket(const ffmpeg::Packet *packet);

private:
    nxpt::CommonRefManager m_refManager;
    std::unique_ptr<TranscodeStreamReader> m_transcodeReader;
    int m_encoderIndex;
    std::shared_ptr<Camera> m_camera;
    CyclicAllocator m_allocator;
    std::atomic_bool m_interrupted = false;
    bool m_isPrimaryStream = true;
};

} // namespace usb_cam::nx
