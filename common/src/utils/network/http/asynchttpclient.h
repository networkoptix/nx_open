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

        \note This class methods are not thread-safe
        \note All signals are emitted from io::AIOService threads
        \note State is changed just before emitting signal
        \warning It is strongly recommended to listen for \a AsyncHttpClient::someMessageBodyAvailable() signal and
            read current message body buffer with a \a AsyncHttpClient::fetchMessageBodyBuffer() call every time
        \todo pipelining support
        \todo keep-alive connection support
        \todo Ability to suspend message body receiving
    */
    class AsyncHttpClient
    :
        public QObject,
        public aio::AIOEventHandler,
        public std::enable_shared_from_this<AsyncHttpClient>
    {
        Q_OBJECT

    public:
        // TODO: #Elric #enum
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

        //!Stops socket event processing. If some event handler is running in a thread different from current one, method blocks until event handler had returned
        /*!
            \note No signal is emitted after this call
        */
        virtual void terminate();

        State state() const;
        //!Returns true, if \a AsyncHttpClient::state() == \a AsyncHttpClient::sFailed
        bool failed() const;
        //!Start GET request to \a url
        /*!
            \return true, if socket is created and async connect is started. false otherwise
            To get error description use SystemError::getLastOSErrorCode()
        */
        bool doGet( const QUrl& url );
        //!Start POST request to \a url
        /*!
            \todo Infinite POST message body support
            \return true, if socket is created and async connect is started. false otherwise
        */
        bool doPost(
            const QUrl& url,
            const nx_http::StringType& contentType,
            const nx_http::StringType& messageBody );
        /*!
            Response is valid only after signal \a responseReceived() has been emitted
            \return Can be NULL if no response has been received yet
        */
        const Response* response() const;
        StringType contentType() const;
        //!Returns current message body buffer, clearing it
        /*!
            \note This method is thread-safe and can be called in any thread
        */
        BufferType fetchMessageBodyBuffer();
        const QUrl& url() const;
        //!Number of total bytes read (including http request line and headers)
        quint64 totalBytesRead() const;

        //!By default, entity compression is on
        void setUseCompression( bool toggleUseEntityEncoding );
        void setSubsequentReconnectTries( int reconnectTries );
        void setTotalReconnectTries( int reconnectTries );
        void setUserAgent( const QString& userAgent );
        void setUserName( const QString& userAgent );
        void setUserPassword( const QString& userAgent );
        void setResponseReadTimeoutMs( int _responseReadTimeoutMs );
        /*!
            By default \a true.
            \param val If \a false, chunked message is not decoded and returned as-is by \a AsyncHttpClient::fetchMessageBodyBuffer
        */
        void setDecodeChunkedMessageBody( bool val );

        QSharedPointer<AbstractStreamSocket> takeSocket();

        void addRequestHeader(const StringType& key, const StringType& value);

    signals:
        void tcpConnectionEstablished( nx_http::AsyncHttpClientPtr );
        //!Emitted when response headers has been read
        void responseReceived( nx_http::AsyncHttpClientPtr );
        //!Message body buffer is not empty
        /*!
            Received message body buffer is appended to internal buffer which can be read with \a AsyncHttpClient::fetchMessageBodyBuffer() call.
            Responsibility for preventing internal message body buffer to grow beyond reasonable sizes lies on user of this class.
            \warning It is strongly recommended to call \a AsyncHttpClient::fetchMessageBodyBuffer() every time on receiving this signal
        */
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
        virtual void eventTriggered( AbstractSocket* sock, aio::EventType eventType ) throw() override;

    private:
        State m_state;
        Request m_request;
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
        bool m_terminated;
        mutable QMutex m_mutex;
        quint64 m_totalBytesRead;
        bool m_contentEncodingUsed;
        int m_responseReadTimeoutMs;

        void resetDataBeforeNewRequest();
        bool initiateHttpMessageDelivery( const QUrl& url );
        /*!
            \return Number of bytes, read from socket. -1 in case of read error
        */
        int readAndParseHttp();
        void composeRequest( const nx_http::StringType& httpMethod );
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
        bool resendRequestWithAuthorization( const nx_http::Response& response );
        void eventTriggeredPrivate( AbstractSocket* sock, aio::EventType eventType );

        static const char* toString( State state );
    };
}

#endif  //ASYNCHTTPCLIENT_H
