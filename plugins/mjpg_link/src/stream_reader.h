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

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <plugins/plugin_container_api.h>
#include <utils/memory/cyclic_allocator.h>

#include "ilp_video_packet.h"


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
    nxcip::CameraInfo m_cameraInfo;
    float m_fps;
    int m_encoderNumber;
    nxcip::UsecUTCTimestamp m_curTimestamp;
    std::shared_ptr<nx::network::http::HttpClient> m_httpClient;
    std::unique_ptr<nx::network::http::MultipartContentParser> m_multipartContentParser;
    std::unique_ptr<ILPVideoPacket> m_videoPacket;
    StreamType m_streamType;
    qint64 m_prevFrameClock;
    qint64 m_frameDurationMSec;
    bool m_terminated;
    QnWaitCondition m_cond;
    QnMutex m_mutex;
    std::atomic<int> m_isInGetNextData;
    nxpl::TimeProvider* const m_timeProvider;
 
    int doRequest( nx::network::http::HttpClient* const httpClient );
    void gotJpegFrame( const nx::network::http::ConstBufferRefType& jpgFrame );
    /*!
        \return false, if has been interrupted. Otherwise \a true
    */
    bool waitForNextFrameTime();
};

#endif  //ILP_STREAM_READER_H
