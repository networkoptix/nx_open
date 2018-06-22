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
#include <plugins/plugin_container_api.h>
#include <utils/memory/cyclic_allocator.h>

#include "ilp_video_packet.h"
#include "codec_context.h"
#include "ffmpeg/forward_declarations.h"

namespace nx {
namespace webcam_plugin {

namespace ffmpeg { class StreamReader; }

class CodecContext;

//!Transfers or transcodes packets from USB webcameras and streams them
class StreamReader
:
    public nxcip::StreamReader
{

protected:
    class TimeProfiler
    {
        typedef std::chrono::high_resolution_clock::time_point timepoint;
        timepoint startTime;
        timepoint stopTime;
    
    public:
        timepoint now()
        {
            return std::chrono::high_resolution_clock::now();
        }

        void start()
        {
            startTime = now();
        }

        void stop()
        {
            stopTime = now();
        }

        std::chrono::nanoseconds elapsed()
        {
            return stopTime - startTime;
        }

        int64_t countMsec()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed()).count();
            
        }

        int64_t countUsec()
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(elapsed()).count();
        }

        int64_t countNsec()
        {
            return elapsed().count();
        }
    };
    
public:
    StreamReader(
        nxpt::CommonRefManager* const parentRefManager,
        nxpl::TimeProvider *const timeProvider,
        const nxcip::CameraInfo& cameraInfo,
        const CodecContext& codecContext,
        const std::weak_ptr<ffmpeg::StreamReader>& ffmpegStreamReader);
    virtual ~StreamReader();

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

    virtual int getNextData( nxcip::MediaDataPacket** packet ) = 0;
    virtual void interrupt() override;

    virtual void setFps( int fps ) = 0;
    virtual void setResolution(const nxcip::Resolution& resolution) = 0;
    virtual void setBitrate(int bitrate) = 0;
    void updateCameraInfo( const nxcip::CameraInfo& info );

protected:
    nxpt::CommonRefManager m_refManager;
    nxpl::TimeProvider* const m_timeProvider;
    CyclicAllocator m_allocator;
    nxcip::CameraInfo m_info;
    CodecContext m_codecContext;
    
    QnMutex m_mutex;

    std::weak_ptr<ffmpeg::StreamReader> m_ffmpegStreamReader;

protected:
    std::unique_ptr<ILPVideoPacket> toNxPacket(AVPacket *packet, AVCodecID codecID);
    //QString decodeCameraInfoUrl() const;
};

} // namespace webcam_plugin
} // namespace nx
