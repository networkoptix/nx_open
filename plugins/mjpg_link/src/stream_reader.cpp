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

#include <QtCore/QElapsedTimer>
#include <QtCore/QThread>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/byte_stream/custom_output_stream.h>

#include "ilp_empty_packet.h"
#include "motion_data_picture.h"
#include "plugin.h"


static const nxcip::UsecUTCTimestamp USEC_IN_MS = 1000;
static const nxcip::UsecUTCTimestamp USEC_IN_SEC = 1000*1000;
static const nxcip::UsecUTCTimestamp NSEC_IN_USEC = 1000;
static const int MAX_FRAME_SIZE = 4*1024*1024;

StreamReader::StreamReader(
    nxpt::CommonRefManager* const parentRefManager,
    nxpl::TimeProvider *const timeProvider,
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
    m_terminated( false ),
    m_isInGetNextData( 0 ),
    m_timeProvider(timeProvider)
{
    NX_ASSERT(m_timeProvider);
    setFps( fps );
}

StreamReader::~StreamReader()
{
    NX_ASSERT( m_isInGetNextData == 0 );
    m_timeProvider->releaseRef();

    m_videoPacket.reset();
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

static const unsigned int MAX_FRAME_DURATION_MS = 5000;

int StreamReader::getNextData( nxcip::MediaDataPacket** lpPacket )
{
    ++m_isInGetNextData;
    auto SCOPED_GUARD_FUNC = [this]( StreamReader* ) { --m_isInGetNextData; };
    const std::unique_ptr<StreamReader, decltype(SCOPED_GUARD_FUNC)>
        SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

    bool httpClientHasBeenJustCreated = false;
    std::shared_ptr<nx_http::HttpClient> localHttpClientPtr;
    {
        QnMutexLocker lk( &m_mutex );
        if( !m_httpClient )
        {
            m_httpClient = std::make_shared<nx_http::HttpClient>();
            m_httpClient->setMessageBodyReadTimeoutMs( MAX_FRAME_DURATION_MS );
            httpClientHasBeenJustCreated = true;
        }
        localHttpClientPtr = m_httpClient;
    }

    if( httpClientHasBeenJustCreated )
    {
        using namespace std::placeholders;

        m_multipartContentParser = std::make_unique<nx_http::MultipartContentParser>();
        auto jpgFrameHandleFunc = std::bind(&StreamReader::gotJpegFrame, this, _1);
        m_multipartContentParser->setNextFilter(
            std::make_shared<nx::utils::bstream::CustomOutputStream<decltype(jpgFrameHandleFunc)>>(
                jpgFrameHandleFunc));

        const int result = doRequest( localHttpClientPtr.get() );
        if( result != nxcip::NX_NO_ERROR )
            return result;

        if( nx_http::strcasecmp(localHttpClientPtr->contentType(), "image/jpeg") == 0 )
        {
            //single jpeg, have to get it by timer
            m_streamType = jpg;
        }
        else if( m_multipartContentParser->setContentType(localHttpClientPtr->contentType()) )
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
            if( !httpClientHasBeenJustCreated )
            {
                if( !waitForNextFrameTime() )
                    return nxcip::NX_INTERRUPTED;
                const int result = doRequest( localHttpClientPtr.get() );
                if( result != nxcip::NX_NO_ERROR )
                    return result;
            }

            nx_http::BufferType msgBody;
            while( msgBody.size() < MAX_FRAME_SIZE )
            {
                const nx_http::BufferType& msgBodyBuf = localHttpClientPtr->fetchMessageBodyBuffer();
                {
                    QnMutexLocker lk( &m_mutex );
                    if( m_terminated )
                    {
                        m_terminated = false;
                        return nxcip::NX_INTERRUPTED;
                    }
                }
                if( localHttpClientPtr->eof() )
                {
                    gotJpegFrame( msgBody.isEmpty() ? msgBodyBuf : (msgBody + msgBodyBuf) );
                    localHttpClientPtr.reset();
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
            {
                m_multipartContentParser->processData(localHttpClientPtr->fetchMessageBodyBuffer());
                {
                    QnMutexLocker lk( &m_mutex );
                    if( m_terminated )
                    {
                        m_terminated = false;
                        return nxcip::NX_INTERRUPTED;
                    }
                }
            }
            break;

        default:
            break;
    }

    if( m_videoPacket.get() )
    {
        *lpPacket = m_videoPacket.release();
        return nxcip::NX_NO_ERROR;
    }

    QnMutexLocker lk( &m_mutex );
    if( m_httpClient )
    {
        //reconnecting
        m_httpClient.reset();
    }

    return nxcip::NX_NETWORK_ERROR;
}

void StreamReader::interrupt()
{
    std::shared_ptr<nx_http::HttpClient> client;
    {
        QnMutexLocker lk(&m_mutex);
        m_terminated = true;
        m_cond.wakeAll();
        std::swap(client, m_httpClient);
    }

    //closing connection
    if(client)
    {
        client->pleaseStop();
        client.reset();
    }
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

int StreamReader::doRequest( nx_http::HttpClient* const httpClient )
{
    httpClient->setUserName( QLatin1String(m_cameraInfo.defaultLogin) );
    httpClient->setUserPassword( QLatin1String(m_cameraInfo.defaultPassword) );
    if( !httpClient->doGet( nx::utils::Url(QLatin1String(m_cameraInfo.url)) ) ||
        !httpClient->response() )
    {
        NX_LOG( QString::fromLatin1("Failed to request %1").arg(QLatin1String(m_cameraInfo.url)), cl_logDEBUG1 );
        return nxcip::NX_NETWORK_ERROR;
    }
    if( httpClient->response()->statusLine.statusCode == nx_http::StatusCode::unauthorized )
    {
        NX_LOG( QString::fromLatin1("Failed to request %1: %2").arg(QLatin1String(m_cameraInfo.url)).
            arg(QLatin1String(httpClient->response()->statusLine.reasonPhrase)), cl_logDEBUG1 );
        return nxcip::NX_NOT_AUTHORIZED;
    }
    if( httpClient->response()->statusLine.statusCode / 100 * 100 != nx_http::StatusCode::ok )
    {
        NX_LOG( QString::fromLatin1("Failed to request %1: %2").arg(QLatin1String(m_cameraInfo.url)).
            arg(QLatin1String(httpClient->response()->statusLine.reasonPhrase)), cl_logDEBUG1 );
        return nxcip::NX_NETWORK_ERROR;
    }
    return nxcip::NX_NO_ERROR;
}

void StreamReader::gotJpegFrame( const nx_http::ConstBufferRefType& jpgFrame )
{
    //creating video packet

    m_videoPacket.reset( new ILPVideoPacket(
        &m_allocator,
        0,
        m_timeProvider->millisSinceEpoch()  * USEC_IN_MS,
        nxcip::MediaDataPacket::fKeyPacket,
        0 ) );
    m_videoPacket->resizeBuffer( jpgFrame.size() );
    if( m_videoPacket->data() )
        memcpy( m_videoPacket->data(), jpgFrame.constData(), jpgFrame.size() );
}

bool StreamReader::waitForNextFrameTime()
{
    const qint64 currentTime = m_timeProvider->millisSinceEpoch();
    if( m_prevFrameClock != -1 &&
        !((m_prevFrameClock > currentTime) || (currentTime - m_prevFrameClock > m_frameDurationMSec)) ) //system time changed
    {
        const qint64 msecToSleep = m_frameDurationMSec - (currentTime - m_prevFrameClock);
        if( msecToSleep > 0 )
        {
            QnMutexLocker lk( &m_mutex );
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
    m_prevFrameClock = m_timeProvider->millisSinceEpoch();
    return true;
}
