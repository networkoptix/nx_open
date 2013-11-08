/**********************************************************
* 22 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef ASYNCHTTPCLIENT_H
#define ASYNCHTTPCLIENT_H

#include <map>
#include <memory>

#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QSharedPointer>

#include "httpstreamreader.h"
#include "../aio/aioeventhandler.h"


namespace nx_http
{
    class AsyncHttpClient;
    typedef std::shared_ptr<AsyncHttpClient> AsyncHttpClientPtr;

    //!Http client. All operations are done asynchronously using aio::AIOService
    /*!
        It is strongly recommended to connect to signals using Qt::DirectConnection and slot should not use blocking calls.
        
        \warning Instance of \a AsyncHttpClient MUST be used as shared pointer (std::shared_ptr)

        \note On receiving reply, client will not start downloading message body until \a readMessageBody() call
        \note This class methods are not thread-safe
        \note All signals are emitted from io::AIOService threads
        \note State is changed just before emitting signal
        \todo pipelining support
        \todo keep-alive connection support
        \todo entity-body compression support
    */
    class AsyncHttpClient
    :
        public QObject,
        public aio::AIOEventHandler,
        public std::enable_shared_from_this<AsyncHttpClient>
    {
        Q_OBJECT

    public:
        enum State
        {
            sInit,
            sWaitingConnectToHost,
            sSendingRequest,
            sReceivingResponse,
            sResponseReceived,
            sReadingMessageBody,
            sFailed,
            sDone
        };

        static const int UNLIMITED_RECONNECT_TRIES = -1;

        AsyncHttpClient();
        virtual ~AsyncHttpClient();

        //!Stops socket event processing. If some event handler is running, method blocks until event handler has been stopped
        virtual void terminate();

        State state() const;
        //!Returns true, if \a AsyncHttpClient::state() == \a AsyncHttpClient::sFailed
        bool failed() const;
        //!Start request to \a url
        /*!
            \return true, if socket is created and async connect is started. false otherwise
            To get error description use SystemError::getLastOSErrorCode()
        */
        bool doGet( const QUrl& url );
        /*!
            Response is valid only after signal \a responseReceived() has been emitted
            \return Can be NULL if no response has been received yet
        */
        const HttpResponse* response() const;
        StringType contentType() const;
        //!Start receiving message body
        /*!
            \return false if failed to start reading message body
        */
        bool startReadMessageBody();
        //!Returns current message body buffer, clearing it
        /*!
            \note This method is thread-safe and can be called in any thread
        */
        BufferType fetchMessageBodyBuffer();
        const QUrl& url() const;
        //!Number of total bytes read (including http request line and headers)
        quint64 totalBytesRead() const;

        void setSubsequentReconnectTries( int reconnectTries );
        void setTotalReconnectTries( int reconnectTries );
        void setUserAgent( const QString& userAgent );
        void setUserName( const QString& userAgent );
        void setUserPassword( const QString& userAgent );

    signals:
        void tcpConnectionEstablished( nx_http::AsyncHttpClientPtr );
        //!Emitted when response headers has been read
        void responseReceived( nx_http::AsyncHttpClientPtr );
        //!Message body buffer is not empty
        void someMessageBodyAvailable( nx_http::AsyncHttpClientPtr );
        /*!
            Emmitted when http request is done with any result (successfully executed request and received message body, 
            received response with error code, connection terminated unexpectedly).
            To get result code use method \a response()
            \note Some message body can still be stored in internal buffer. To read it, call \a AsyncHttpClient::fetchMessageBodyBuffer
        */
        void done( nx_http::AsyncHttpClientPtr );
        //!Connection to server has been restored after a sudden disconnect
        void reconnected( nx_http::AsyncHttpClientPtr );

    protected:
        //!Implementation of aio::AIOEventHandler::eventTriggered
        virtual void eventTriggered( AbstractSocket* sock, PollSet::EventType eventType ) throw() override;

    private:
        State m_state;
        HttpRequest m_request;
        QSharedPointer<AbstractStreamSocket> m_socket;
        BufferType m_requestBuffer;
        size_t m_requestBytesSent;
        QUrl m_url;
        HttpStreamReader m_httpStreamReader;
        BufferType m_responseBuffer;
        QString m_userAgent;
        QString m_userName;
        QString m_userPassword;
        bool m_authorizationTried;
        std::map<BufferType, BufferType> m_customHeaders;
        bool m_terminated;
        mutable QMutex m_mutex;
        quint64 m_totalBytesRead;

        bool doGetPrivate( const QUrl& url );
        /*!
            \return Number of bytes, read from socket. -1 in case of read error
        */
        int readAndParseHttp();
        void formRequest();
        void serializeRequest();
        //!Sends request through \a m_socket
        /*!
            This method performs exactly one non-blocking send call and updates m_requestBytesSent by sent bytes.
            Whole request is sent if \a m_requestBytesSent == \a m_requestBuffer.size()
            \return false in case of send error
        */
        bool sendRequest();
        /*!
            \return true, if connected
        */
        bool reconnectIfAppropriate();
        //!Composes request with authorization header based on \a response
        bool resendRequestWithAuthorization( const nx_http::HttpResponse& response );
        void eventTriggeredPrivate( AbstractSocket* sock, PollSet::EventType eventType );

        static const char* toString( State state );
    };
}

#endif  //ASYNCHTTPCLIENT_H
