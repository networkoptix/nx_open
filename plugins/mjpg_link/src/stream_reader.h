/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef ILP_STREAM_READER_H
#define ILP_STREAM_READER_H

#include <memory>

#include <QtCore/QMutex>
#include <QtCore/QUrl>
#include <QtCore/QWaitCondition>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <utils/network/http/httpclient.h>
#include <utils/network/http/multipart_content_parser.h>
#include <utils/memory/cyclic_allocator.h>

#include "ilp_video_packet.h"


//!Reads picture files from specified directory as video-stream
class StreamReader
:
    public nxcip::StreamReader
{
public:
    StreamReader(
        nxpt::CommonRefManager* const parentRefManager,
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

private:
    enum StreamType
    {
        none,
        mjpg,
        jpg
    };

    nxpt::CommonRefManager m_refManager;
    nxcip::CameraInfo m_cameraInfo;
    float m_fps;
    int m_encoderNumber;
    nxcip::UsecUTCTimestamp m_curTimestamp;
    QSharedPointer<nx_http::HttpClient> m_httpClient;
    nx_http::MultipartContentParser m_multipartContentParser;
    std::unique_ptr<ILPVideoPacket> m_videoPacket;
    StreamType m_streamType;
    qint64 m_prevFrameClock;
    qint64 m_frameDurationMSec;
    bool m_terminated;
    QWaitCondition m_cond;
    QMutex m_mutex;
    CyclicAllocator m_allocator;
 
    void gotJpegFrame( const nx_http::ConstBufferRefType& jpgFrame );
    /*!
        \return false, if has been interrupted. Otherwise \a true
    */
    bool waitForNextFrameTime();
};

#endif  //ILP_STREAM_READER_H
