/**********************************************************
* 22 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef ASYNCHTTPCLIENT_H
#define ASYNCHTTPCLIENT_H

#include <map>

#include <QMutex>
#include <QObject>
#include <QUrl>
#include <QSharedPointer>

#include "httpstreamreader.h"
#include "../aio/aioeventhandler.h"
#include "../aio/selfremovable.h"


namespace nx_http
{
    //!Http client. All operations are done asynchronously using aio::AIOService
    /*!
        It is strongly recommended to connect to signals using Qt::DirectConnection and slot should not use blocking calls.
        Object can be freed from signal handler by calling SelfRemovable::scheduleForRemoval

        \note On receiving reply, client will not start downloading message body until \a readMessageBody() call
        \note This class methods are not thread-safe
        \note All signals are emitted from aio::AIOService threads
        \note State is changed just before emitting signal
        \todo pipelining support
        \todo keep-alive connection support
    */
    class AsyncHttpClient
    :
        public QObject,
        public aio::AIOEventHandler,
        public SelfRemovable
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

        //!Stops socket event processing. If some event handler is running, method blocks until event handler has been stopped
        virtual void terminate();

        State state() const;
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
        const Response* response() const;
        StringType contentType() const;
        //!Start receiving message body
        /*!
            \return false if failed to start reading message body
        */
        bool startReadMessageBody();
        //!Returns current mesasge body buffer, clearing it
        /*!
            \note This method can be called only from slot directly connected to \a someMessageBodyAvailable()
        */
        BufferType fetchMessageBodyBuffer();
        const QUrl& url() const;

        void setSubsequentReconnectTries( int reconnectTries );
        void setTotalReconnectTries( int reconnectTries );
        void setUserAgent( const QString& userAgent );
        void setUserName( const QString& userAgent );
        void setUserPassword( const QString& userAgent );

    signals:
        void tcpConnectionEstablished( nx_http::AsyncHttpClient* );
        //!Emitted when response headers has been read
        void responseReceived( nx_http::AsyncHttpClient* );
        //!Message body buffer is not empty
        void someMessageBodyAvailable( nx_http::AsyncHttpClient* );
        /*!
            Emmitted when http request is done with any result (successfully executed request and received message body, 
            received response with error code, connection terminated enexpectedly).
            To get result code use method \a response()
        */
        void done( nx_http::AsyncHttpClient* );
        //!Connection to server has been restored after a sudden disconnect
        void reconnected( nx_http::AsyncHttpClient* );

    protected:
        virtual ~AsyncHttpClient();

        //!Implementation of aio::AIOEventHandler::eventTriggered
        virtual void eventTriggered( Socket* sock, PollSet::EventType eventType ) override;

    private:
        State m_state;
        Request m_request;
        QSharedPointer<TCPSocket> m_socket;
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
        bool m_inEventHandler;

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
        bool resendRequstWithAuthorization( const nx_http::Response& response );

        static const char* toString( State state );
    };
}

#endif  //ASYNCHTTPCLIENT_H
