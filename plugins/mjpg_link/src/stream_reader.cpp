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
#include <functional>
#include <memory>

#include <QtCore/QDateTime>
#include <QtCore/QElapsedTimer>
#include <QtCore/QMutexLocker>
#include <QtCore/QThread>

#include <utils/common/log.h>
#include <utils/media/custom_output_stream.h>

#include "ilp_empty_packet.h"
#include "motion_data_picture.h"


static const nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
static const nxcip::UsecUTCTimestamp USEC_IN_SEC = 1000*1000;
static const nxcip::UsecUTCTimestamp NSEC_IN_USEC = 1000;
static const int MAX_FRAME_SIZE = 4*1024*1024;

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    const nxcip::CameraInfo& cameraInfo,
    float fps,
    int encoderNumber )
:
    m_refManager( parentRefManager ),
    m_cameraInfo( cameraInfo ),
    m_fps( fps ),
    m_encoderNumber( encoderNumber ),
    m_curTimestamp( 0 ),
    m_streamType( none ),
    m_prevFrameClock( -1 ),
    m_frameDurationMSec( 0 ),
    m_terminated( false )
{
    setFps( fps );

    using namespace std::placeholders;

    auto jpgFrameHandleFunc = std::bind( &StreamReader::gotJpegFrame, this, _1 );
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
    bool needToInitializeHttpClient = false;
    QSharedPointer<nx_http::HttpClient> localHttpClientPtr;
    {
        QMutexLocker lk( &m_mutex );
        if( !m_httpClient )
        {
            m_httpClient = QSharedPointer<nx_http::HttpClient>( new nx_http::HttpClient() );
            needToInitializeHttpClient = true;
        }
        localHttpClientPtr = m_httpClient;
    }

    if( needToInitializeHttpClient )
    {
        if( m_streamType == jpg )
            if( !waitForNextFrameTime() )
                return nxcip::NX_INTERRUPTED;
        localHttpClientPtr->setUserName( QLatin1String(m_cameraInfo.defaultLogin) );
        localHttpClientPtr->setUserPassword( QLatin1String(m_cameraInfo.defaultPassword) );
        if( !localHttpClientPtr->doGet( QUrl(QLatin1String(m_cameraInfo.url)) ) ||
            !localHttpClientPtr->response() )
        {
            NX_LOG( QString::fromLatin1("Failed to request %1").arg(QLatin1String(m_cameraInfo.url)), cl_logDEBUG1 );
            return nxcip::NX_NETWORK_ERROR;
        }
        if( localHttpClientPtr->response()->statusLine.statusCode == nx_http::StatusCode::unauthorized )
        {
            NX_LOG( QString::fromLatin1("Failed to request %1: %2").arg(QLatin1String(m_cameraInfo.url)).
                arg(QLatin1String(localHttpClientPtr->response()->statusLine.reasonPhrase)), cl_logDEBUG1 );
            return nxcip::NX_NOT_AUTHORIZED;
        }
        if( localHttpClientPtr->response()->statusLine.statusCode / 100 * 100 != nx_http::StatusCode::ok )
        {
            NX_LOG( QString::fromLatin1("Failed to request %1: %2").arg(QLatin1String(m_cameraInfo.url)).
                arg(QLatin1String(localHttpClientPtr->response()->statusLine.reasonPhrase)), cl_logDEBUG1 );
            return nxcip::NX_NETWORK_ERROR;
        }

        if( nx_http::strcasecmp(localHttpClientPtr->contentType(), "image/jpeg") == 0 )
        {
            //single jpeg, have to get it by timer
            m_streamType = jpg;
        }
        else if( m_multipartContentParser.setContentType(localHttpClientPtr->contentType()) )
        {
            //motion jpeg stream
            m_streamType = mjpg;
        }
        else
        {
            NX_LOG( QString::fromLatin1("Unsupported Content-Type %1").arg(QLatin1String(localHttpClientPtr->contentType())), cl_logDEBUG1 );
            return nxcip::NX_UNSUPPORTED_CODEC;
        }
    }

    switch( m_streamType )
    {
        case jpg:
        {
            nx_http::BufferType msgBody;
            while( msgBody.size() < MAX_FRAME_SIZE )
            {
                const nx_http::BufferType& msgBodyBuf = localHttpClientPtr->fetchMessageBodyBuffer();
                if( localHttpClientPtr->eof() )
                {
                    gotJpegFrame( msgBody.isEmpty() ? msgBodyBuf : (msgBody + msgBodyBuf) );
                    localHttpClientPtr.reset();   //TODO #ak reuse existing http client
                    break;
                }
                else
                {
                    msgBody += msgBodyBuf;
                }
            }
            break;
        }

        case mjpg:
            //reading mjpg picture
            while( !m_videoPacket.get() && !localHttpClientPtr->eof() )
                m_multipartContentParser.processData( localHttpClientPtr->fetchMessageBodyBuffer() );
            break;

        default:
            break;
    }

    if( m_videoPacket.get() )
    {
        *lpPacket = m_videoPacket.release();
        return nxcip::NX_NO_ERROR;
    }

    QMutexLocker lk( &m_mutex );
    if( m_httpClient )
    {
        //reconnecting
        m_httpClient.reset();
    }

    return nxcip::NX_NETWORK_ERROR;
}

void StreamReader::interrupt()
{
    QMutexLocker lk( &m_mutex );
    m_terminated = true;
    m_cond.wakeAll();

    //closing connection
    if( m_httpClient )
        m_httpClient.reset();
}

void StreamReader::setFps( float fps )
{
    m_fps = fps;
    m_frameDurationMSec = (qint64)(1000.0 / m_fps);
}

void StreamReader::updateCameraInfo( const nxcip::CameraInfo& info )
{
    m_cameraInfo = info;
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
}

bool StreamReader::waitForNextFrameTime()
{
    const qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if( m_prevFrameClock != -1 &&
        !((m_prevFrameClock > currentTime) || (currentTime - m_prevFrameClock > m_frameDurationMSec)) ) //system time changed
    {
        const qint64 msecToSleep = m_frameDurationMSec - (currentTime - m_prevFrameClock);
        if( msecToSleep > 0 )
        {
            QMutexLocker lk( &m_mutex );
            QElapsedTimer monotonicTimer;
            monotonicTimer.start();
            qint64 msElapsed = monotonicTimer.elapsed();
            while( !m_terminated && (msElapsed < msecToSleep) )
            {
                m_cond.wait( lk.mutex(), msecToSleep - msElapsed );
                msElapsed = monotonicTimer.elapsed();
            }
            if( m_terminated )
            {
                //call has been interrupted
                m_terminated = false;
                return false;
            }
        }
    }
    m_prevFrameClock = QDateTime::currentMSecsSinceEpoch();
    return true;
}
