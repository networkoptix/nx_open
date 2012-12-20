////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef HLS_SERVER_H
#define HLS_SERVER_H

#include <QDateTime>

#include <utils/network/http/httpstreamreader.h>
#include <utils/network/tcp_connection_processor.h>


class QnHttpLiveStreamingProcessor
:
    virtual public QnTCPConnectionProcessor
{
public:
    QnHttpLiveStreamingProcessor( TCPSocket* socket, QnTcpListener* owner );
    virtual ~QnHttpLiveStreamingProcessor();

protected:
    virtual void run() override;

private:
    enum State
    {
        sReceiving,
        sProcessingMessage,
        sSending,
        sDone
    };

    nx_http::HttpStreamReader m_httpStreamReader;
    State m_state;
    //!Used to accept/send data
    nx_http::BufferType m_msgBuffer;

    /*!
        \return false in case if error
    */
    bool receiveRequest();
    //!Processes \a request, generates and sends (asynchronously) response
    void processRequest( const nx_http::Request& request );
    //!
    /*!
        In case of success, adds Content-Type, Content-Length headers to \a response
    */
    nx_http::StatusCode::Value getRequestedFile( const nx_http::Request& request, nx_http::Response* const response );
    void sendResponse( const nx_http::Response& response );
    /*!
        \return false, if no more data to send
    */
    bool prepareDataToSend();
    nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getHLSPlaylist(
        const QStringRef& uniqueResourceID,
        const std::multimap<QString, QString>& requestParams,
        nx_http::Response* const response );
    nx_http::StatusCode::Value QnHttpLiveStreamingProcessor::getResourceChunk(
        const QStringRef& uniqueResourceID,
        const std::multimap<QString, QString>& requestParams,
        nx_http::Response* const response );
};

#endif  //HLS_SERVER_H
