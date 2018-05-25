#pragma once

#include <atomic>
#include <memory>

#include <QtCore/QUrl>
extern "C" {
#include <libavcodec/avcodec.h>
} //extern "C"

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/multipart_content_parser.h>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_container_api.h>
#include <utils/memory/cyclic_allocator.h>

#include "ilp_video_packet.h"
#include "codec_context.h"
#include "libav_forward_declarations.h"

namespace nx {
namespace webcam_plugin {

class CodecContext;
class AVCodecContainer;

//!Transfers or transcodes packets from USB based webcameras and streams them
class StreamReader
:
    public nxcip::StreamReader
{
public:
    StreamReader(nxpt::CommonRefManager* const parentRefManager,
        nxpl::TimeProvider *const timeProvider,
        const nxcip::CameraInfo& cameraInfo,
        int encoderNumber,
        const CodecContext& codecContext);
    virtual ~StreamReader();

    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    virtual unsigned int addRef() override;
    virtual unsigned int releaseRef() override;

    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;
    virtual void interrupt() override;

    void setFps( float fps );
    void setResolution(const nxcip::Resolution& resolution);
    void setBitrate(int64_t bitrate);
    void updateCameraInfo( const nxcip::CameraInfo& info );

    bool isNative() const;
private:
    nxpt::CommonRefManager m_refManager;
    nxpl::TimeProvider* const m_timeProvider;

    CyclicAllocator m_allocator;
    nxcip::CameraInfo m_info;
    int m_encoderNumber;
    
    CodecContext m_codecContext;
    bool m_initialized;
    bool m_modified;
    bool m_terminated;
    QnMutex m_mutex;

    bool m_transcodingNeeded;
    AVFormatContext * m_formatContext;
    AVInputFormat * m_inputFormat;
    AVDictionary * m_formatContextOptions;
    AVPacket* m_avDecodePacket;
    std::unique_ptr<AVCodecContainer> m_videoEncoder;
    std::unique_ptr<AVCodecContainer> m_videoDecoder;
    int m_lastAVError = 0;

private:
    std::unique_ptr<ILPVideoPacket> toNxVideoPacket(AVPacket *packet, AVCodecID codecID);
    std::unique_ptr<ILPVideoPacket> transcodeVideo(int *nxcipErrorCode, AVPacket * decodePacket);
    
    int decodeVideoFrame(AVFrame** outFrame, AVPacket* decodePacket);
    AVFrame* toEncodableFrame(AVFrame* frame) const;
    QString decodeCameraInfoUrl() const;


    bool isValid() const;
    void initializeAV();
    int openInputFormat();
    int openVideoDecoder();
     void setEncoderOptions(AVCodecContext* encoderContext);
    int openVideoEncoder();
    void setFormatContextOptions();
    void uninitializeAV();
    bool ensureInitialized();

    void setAVErrorCode(int avErrorCode);
    bool updateIfAVError(int avErrorCode);

    const char * getAVInputFormat();
    std::string getAVCameraUrl();
};

} // namespace webcam_plugin
} // namespace nx
