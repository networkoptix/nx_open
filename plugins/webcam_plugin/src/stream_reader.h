/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef ILP_STREAM_READER_H
#define ILP_STREAM_READER_H

#include <atomic>
#include <memory>

#include <QtCore/QUrl>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/multipart_content_parser.h>

#include <camera/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_container_api.h>
#include <utils/memory/cyclic_allocator.h>

#include "ilp_video_packet.h"
#include "av_string_error.h"

#include <libavutil/pixfmt.h>
#include "libav_forward_declarations.h"

extern "C"
{
    #include <libavcodec/avcodec.h>
}

class AVCodecContainer;

//!Reads picture files from specified directory as video-stream
class StreamReader
:
    public nxcip::StreamReader
{
public:
    StreamReader(nxpt::CommonRefManager* const parentRefManager,
                 nxpl::TimeProvider *const timeProvider,
                 const nxcip::CameraInfo& cameraInfo,
                 float fps,
                 int encoderNumber );
    virtual ~StreamReader();

    //!Implementation of nxpl::PluginInterface::queryInterface
    virtual void* queryInterface( const nxpl::NX_GUID& interfaceID ) override;
    //!Implementation of nxpl::PluginInterface::addRef
    virtual unsigned int addRef() override;
    //!Implementation of nxpl::PluginInterface::releaseRef
    virtual unsigned int releaseRef() override;

    //!Implementation nxcip::StreamReader::getNextData
    virtual int getNextData( nxcip::MediaDataPacket** packet ) override;
    //!Implementation nxcip::StreamReader::interrupt
    virtual void interrupt() override;

    void setFps( float fps );
    void updateCameraInfo( const nxcip::CameraInfo& info );

private:
    enum StreamType
    {
        none,
        mjpg,
        jpg
    };

    nxpt::CommonRefManager m_refManager;
    CyclicAllocator m_allocator;
    nxcip::CameraInfo m_info;
    float m_fps;
    int m_encoderNumber;
    nxcip::UsecUTCTimestamp m_curTimestamp;
   
    StreamType m_streamType;
    qint64 m_prevFrameClock;
    qint64 m_frameDurationMSec;
    bool m_terminated;
    QnWaitCondition m_cond;
    QnMutex m_mutex;
    std::atomic<int> m_isInGetNextData;
    nxpl::TimeProvider* const m_timeProvider;
 
    AVFormatContext * m_formatContext;
    AVInputFormat * m_inputFormat;
    AVDictionary * m_options;
    std::unique_ptr<AVCodecContainer> m_videoEncoder;
    std::unique_ptr<AVCodecContainer> m_videoDecoder;

    AVPacket* m_avVideoPacket;

    AVStringError m_lastError;

private:
    std::unique_ptr<ILPVideoPacket> toNxVideoPacket(AVPacket *packet, AVCodecID codecID);
    std::unique_ptr<ILPVideoPacket> transcodeVideo(int *nxcipErrorCode);
    
    AVFrame* getDecodedVideoFrame();
    AVPacket* getEncodedPacket(AVFrame* frame);

    bool isValid();
    void initializeAv();
    void setOptions();
    void setEncoderOptions();
    AVFrame* toYUV420(AVCodecContext* codecContext, AVFrame* frame);

    const char * getAvInputFormat();
    QString getAvCameraUrl();

    //void gotJpegFrame( const nx::network::http::ConstBufferRefType& jpgFrame );
    /*!
        \return false, if has been interrupted. Otherwise \a true
    */
    bool waitForNextFrameTime();
};

#endif  //ILP_STREAM_READER_H
