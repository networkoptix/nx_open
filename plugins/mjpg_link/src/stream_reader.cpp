/**********************************************************
* 04 sep 2013
* akolesnikov@networkoptix.com
***********************************************************/

#include "stream_reader.h"

#ifdef _WIN32
#include <Windows.h>
#undef max
#undef min
#else
#include <time.h>
#include <unistd.h>
#endif

#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <algorithm>
#include <fstream>
#include <memory>

#include <QtCore/QDateTime>

#include <utils/common/log.h>
#include <utils/media/custom_output_stream.h>

#include "ilp_empty_packet.h"
#include "motion_data_picture.h"


static const nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
static const nxcip::UsecUTCTimestamp USEC_IN_SEC = 1000*1000;
static const nxcip::UsecUTCTimestamp NSEC_IN_USEC = 1000;

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    const nxcip::CameraInfo& cameraInfo,
    unsigned int frameDurationUsec,
    int encoderNumber )
:
    m_refManager( parentRefManager ),
    m_cameraInfo( cameraInfo ),
    m_frameDuration( frameDurationUsec ),
    m_encoderNumber( encoderNumber ),
    m_curTimestamp( 0 )
{
    using namespace std::placeholders;

    auto jpgFrameHandleFunc = std::bind( std::mem_fun1( &StreamReader::gotJpegFrame ), this, _1 );
    m_multipartContentParser.setNextFilter( std::make_shared<CustomOutputStream<decltype(jpgFrameHandleFunc)> >(jpgFrameHandleFunc) );
}

StreamReader::~StreamReader()
{
}

//!Implementation of nxpl::PluginInterface::queryInterface
void* StreamReader::queryInterface( const nxpl::NX_GUID& interfaceID )
{
    if( memcmp( &interfaceID, &nxcip::IID_StreamReader, sizeof(nxcip::IID_StreamReader) ) == 0 )
    {
        addRef();
        return this;
    }
    if( memcmp( &interfaceID, &nxpl::IID_PluginInterface, sizeof(nxpl::IID_PluginInterface) ) == 0 )
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return NULL;
}

//!Implementation of nxpl::PluginInterface::addRef
unsigned int StreamReader::addRef()
{
    return m_refManager.addRef();
}

//!Implementation of nxpl::PluginInterface::releaseRef
unsigned int StreamReader::releaseRef()
{
    return m_refManager.releaseRef();
}

int StreamReader::getNextData( nxcip::MediaDataPacket** lpPacket )
{
    static const int MAX_RECONNECT_TRIES = 3;

    for( int i = 0; i < MAX_RECONNECT_TRIES; ++i )
    {
        if( !m_httpClient.get() )
        {
            m_httpClient.reset( new nx_http::HttpClient() );
            m_httpClient->setUserName( QLatin1String(m_cameraInfo.defaultLogin) );
            m_httpClient->setUserPassword( QLatin1String(m_cameraInfo.defaultPassword) );
            if( !m_httpClient->doGet( QUrl(QLatin1String(m_cameraInfo.url)) ) ||
                !m_httpClient->startReadMessageBody() )
            {
                NX_LOG( QString::fromLatin1("Failed to requesting %1").arg(QLatin1String(m_cameraInfo.url)), cl_logDEBUG1 );
                return nxcip::NX_NETWORK_ERROR;
            }

            if (!m_multipartContentParser.setContentType(m_httpClient->contentType()))
            {
                NX_LOG( QString::fromLatin1("Unsupported Content-Type %1").arg(QLatin1String(m_httpClient->contentType())), cl_logDEBUG1 );
                return nxcip::NX_UNSUPPORTED_CODEC;
            }
        }

        //reading mjpg picture
        while( !m_videoPacket.get() && !m_httpClient->eof() )
            m_multipartContentParser.processData( m_httpClient->fetchMessageBodyBuffer() );

        if( m_httpClient->eof() )
        {
            //reconnecting
            m_httpClient.reset();
            continue;
        }

        if( !m_videoPacket.get() )
        {
            //reconnecting
            m_httpClient.reset();
            continue;
        }

        *lpPacket = m_videoPacket.release();
        return nxcip::NX_NO_ERROR;
    }

    return nxcip::NX_NETWORK_ERROR;
}

void StreamReader::interrupt()
{
    //TODO/IMPL
}

void StreamReader::gotJpegFrame( const nx_http::ConstBufferRefType& jpgFrame )
{
    //creating video packet

    m_videoPacket.reset( new ILPVideoPacket(
        0,
        QDateTime::currentMSecsSinceEpoch() * USEC_IN_MS,
        nxcip::MediaDataPacket::fKeyPacket,
        0 ) );
    m_videoPacket->resizeBuffer( jpgFrame.size() );
    if( m_videoPacket->data() )
        memcpy( m_videoPacket->data(), jpgFrame.constData(), jpgFrame.size() );

//#ifdef _DEBUG
//    static int fileNum = 0;
//    std::ofstream of;
//    char fName[_MAX_FNAME];
//    sprintf( fName, "c:\\tmp\\pic\\%d.jpg", ++fileNum );
//    of.open( fName, std::ios_base::out | std::ios_base::binary );
//    of.write( (const char*)m_videoPacket->data(), m_videoPacket->dataSize() );
//#endif
}
