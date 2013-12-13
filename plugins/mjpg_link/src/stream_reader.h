/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef ILP_STREAM_READER_H
#define ILP_STREAM_READER_H

#include <memory>

#include <QtCore/QUrl>

#include <plugins/camera_plugin.h>
#include <plugins/plugin_tools.h>
#include <utils/network/http/httpclient.h>
#include <utils/network/http/multipart_content_parser.h>

#include "ilp_video_packet.h"


class DirContentsManager;

//!Reads picture files from specified directory as video-stream
class StreamReader
:
    public nxcip::StreamReader
{
public:
    StreamReader(
        nxpt::CommonRefManager* const parentRefManager,
        const nxcip::CameraInfo& cameraInfo,
        unsigned int frameDurationUsec,
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

private:
    nxpt::CommonRefManager m_refManager;
    nxcip::CameraInfo m_cameraInfo;
    const unsigned int m_frameDuration;
    int m_encoderNumber;
    nxcip::UsecUTCTimestamp m_curTimestamp;
    std::unique_ptr<nx_http::HttpClient> m_httpClient;
    nx_http::MultipartContentParser m_multipartContentParser;
    std::unique_ptr<ILPVideoPacket> m_videoPacket;

    void gotJpegFrame( const nx_http::ConstBufferRefType& jpgFrame );
};

#endif  //ILP_STREAM_READER_H
