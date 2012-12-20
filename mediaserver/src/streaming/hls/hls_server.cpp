////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "hls_server.h"

#include <algorithm>
#include <map>

#include <utils/common/log.h>


using namespace nx_http;
using namespace std;

static const size_t READ_BUFFER_SIZE = 64*1024;

QnHttpLiveStreamingProcessor::QnHttpLiveStreamingProcessor( TCPSocket* socket, QnTcpListener* owner )
:
    QnTCPConnectionProcessor( socket, owner ),
    m_state( sReceiving )
{
    m_msgBuffer.resize( READ_BUFFER_SIZE );
}

QnHttpLiveStreamingProcessor::~QnHttpLiveStreamingProcessor()
{
}

void QnHttpLiveStreamingProcessor::run()
{
    while( !needToStop() )
    {
        switch( m_state )
        {
            case sReceiving:
                if( !receiveRequest() )
                    return;
                break;

            case sSending:
            {
                Q_ASSERT( !m_msgBuffer.isEmpty() );

                int bytesSent = sendData( m_msgBuffer );
                if( bytesSent < 0 )
                {
                    NX_LOG( QString::fromLatin1("Error sending data to %1 (%2). Terminating connection...").
                        arg(remoteHostAddress()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
                    return;
                }
                m_msgBuffer.remove( 0, bytesSent );
                if( !m_msgBuffer.isEmpty() )
                    break;  //continuing sending
                if( !prepareDataToSend() )
                {
                    NX_LOG( QString::fromLatin1("Finished downloading %1 data to %2. Closing connection...").
                        arg(QLatin1String("entity_path")).arg(remoteHostAddress()), cl_logDEBUG1 );
                    return;
                }
                break;
            }

            case sDone:
                NX_LOG( QString::fromLatin1("Done request to %1. Closing connection...").arg(remoteHostAddress()), cl_logDEBUG1 );
                return;
        }
    }
}

bool QnHttpLiveStreamingProcessor::receiveRequest()
{
    int bytesRead = readSocket( (quint8*)m_msgBuffer.data(), m_msgBuffer.size() );
    if( bytesRead < 0 )
    {
        NX_LOG( QString::fromLatin1("Error reading socket from %1 (%2). Terminating connection...").
            arg(remoteHostAddress()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
        return false;
    }
    if( bytesRead == 0 )
    {
        NX_LOG( QString::fromLatin1("Client %1 closed connection").arg(remoteHostAddress()), cl_logINFO );
        return false;
    }

    size_t bytesProcessed = 0;
    if( !m_httpStreamReader.parseBytes( m_msgBuffer, bytesRead, &bytesProcessed ) )
    {
        NX_LOG( QString::fromLatin1("Error parsing http message from %1 (%2). Terminating connection...").
            arg(remoteHostAddress()).arg(m_httpStreamReader.errorText()), cl_logWARNING );
        return false;
    }
    if( bytesProcessed == m_msgBuffer.size() )
        m_msgBuffer.clear();    //small optimization. may be excessive
    else
        m_msgBuffer.remove( 0, bytesProcessed );

    switch( m_httpStreamReader.state() )
    {
        case HttpStreamReader::readingMessageHeaders:
            //continuing reading
            return true;

        case HttpStreamReader::messageDone:
        case HttpStreamReader::readingMessageBody:
        case HttpStreamReader::waitingMessageStart:
            if( m_httpStreamReader.message().type != MessageType::request )
            {
                NX_LOG( QString::fromLatin1("Received %1 from %2 while expecting request. Terminating connection...").
                    arg(MessageType::toString(m_httpStreamReader.message().type)).arg(remoteHostAddress()), cl_logWARNING );
                return false;
            }
            m_state = sProcessingMessage;
            processRequest( *m_httpStreamReader.message().request );
            return true;

        case HttpStreamReader::parseError:
            //bad request format, terminating connection...
            NX_LOG( QString::fromLatin1("Error parsing http message from %1 (%2). Terminating connection...").
                arg(remoteHostAddress()).arg(m_httpStreamReader.errorText()), cl_logWARNING );
            return false;

        default:
            return false;
    }
}

void QnHttpLiveStreamingProcessor::processRequest( const nx_http::Request& request )
{
    nx_http::Response response;
    response.statusLine.version = request.requestLine.version;
    response.headers.insert( std::make_pair( "Date", "bla-bla-bla" ) );     //TODO/IMPL
    response.headers.insert( std::make_pair( "Server", "bla-bla-bla" ) );
    response.headers.insert( std::make_pair( "Cache-Control", "no-cache" ) );   //findRequestedFile can override this

    //HttpFile file;
    response.statusLine.statusCode = getRequestedFile( request, &response/*, &file*/ );
    if( response.statusLine.reasonPhrase.isEmpty() )
        response.statusLine.reasonPhrase = StatusCode::toString( response.statusLine.statusCode );

    if( request.requestLine.version == nx_http::Version::http_1_1 )
    {
        response.headers.insert( std::make_pair( "Transfer-Encoding", "chunked" ) );
        response.headers.insert( std::make_pair( "Connection", "close" ) ); //no persistent connections support
    }

    sendResponse( response );
}

nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getRequestedFile(
    const nx_http::Request& request,
    nx_http::Response* const response )
{
    //retreiving requested file name
    const QString& path = request.requestLine.url.path();
    int fileNameStartIndex = path.lastIndexOf(QChar('/'));
    if( fileNameStartIndex == -1 )
        return StatusCode::notFound;
    QStringRef fileName;
    if( fileNameStartIndex == path.size()-1 )   //path ends with /. E.g., /hls/camera1.m3u/. Skipping trailing /
    {
#if QT_VERSION >= 0x040800
        int newFileNameStartIndex = path.midRef(0, path.size()-1).lastIndexOf(QChar('/'));
        if( newFileNameStartIndex == -1 )
            return StatusCode::notFound;
#else
        //TODO/IMPL delete after move to Qt4.8
        size_t newFileNameStartIndex = nx_http::find_last_of( path, QChar('/'), 0, path.size()-1 );
        if( newFileNameStartIndex == nx_http::BufferNpos )
            return StatusCode::notFound;
#endif
        fileName = path.midRef( newFileNameStartIndex+1, fileNameStartIndex-(newFileNameStartIndex+1) );
    }
    else
    {
        fileName = path.midRef( fileNameStartIndex+1 );
    }

    //parsing request parameters
    const QList<QPair<QString, QString> >& queryItemsList = request.requestLine.url.queryItems();
    std::multimap<QString, QString> requestParams;
    //moving params to map for more convenient use
    for( QList<QPair<QString, QString> >::const_iterator
        it = queryItemsList.begin();
        it != queryItemsList.end();
        ++it )
    {
        requestParams.insert( std::make_pair( it->first, it->second ) );
    }

    const QChar* extensionSepPos = std::find( fileName.constData(), fileName.constData()+fileName.size(), QChar('.') );
    if( extensionSepPos == fileName.constData()+fileName.size() )
    {
        //no extension, assuming stream has been requested...
        return getResourceChunk( fileName, requestParams, response );
    }
    else
    {
        //found extension
#if QT_VERSION >= 0x040800
        const QStringRef& extension = fileName.mid( extensionSepPos-fileName.constData()+1 );
        const QStringRef& shortFileName = fileName.mid( 0, extensionSepPos-fileName.constData() );
#else
        const QStringRef& extension = fileName.string()->midRef(
            fileName.position()+(extensionSepPos-fileName.constData()+1),
            fileName.size()-(extensionSepPos-fileName.constData()+1) );
        const QStringRef& shortFileName = fileName.string()->midRef(
            fileName.position(),
            extensionSepPos-fileName.constData() );
#endif
        if( extension.compare(QLatin1String("m3u")) == 0 || extension.compare(QLatin1String("m3u8")) == 0 )
            return getHLSPlaylist( shortFileName, requestParams, response );
    }

    return StatusCode::notFound;
}

void QnHttpLiveStreamingProcessor::sendResponse( const nx_http::Response& response )
{
    //serializing response to internal buffer
    m_msgBuffer.clear();
    response.serialize( &m_msgBuffer );

    m_state = sSending;
}

bool QnHttpLiveStreamingProcessor::prepareDataToSend()
{
    //TODO/IMPL preparing media stream to send
    return false;
}

nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getHLSPlaylist(
    const QStringRef& uniqueResourceID,
    const std::multimap<QString, QString>& requestParams,
    nx_http::Response* const response )
{
    response->headers.insert( make_pair("Content-Type", "audio/mpegurl") );
    response->messageBody = "<HTML><body>HUY-HUY-HUY</body></HTML>";
    response->headers.insert( make_pair("Content-Length", QByteArray::number(response->messageBody.size()) ) );

    return nx_http::StatusCode::ok;
}

nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getResourceChunk(
    const QStringRef& uniqueResourceID,
    const std::multimap<QString, QString>& requestParams,
    nx_http::Response* const response )
{
    //TODO/IMPL
    return nx_http::StatusCode::notImplemented;
}
