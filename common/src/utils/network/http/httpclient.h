/**********************************************************
* 26 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include "asynchttpclient.h"

#include <utils/thread/wait_condition.h>
#include <utils/thread/mutex.h>

#include <utils/common/stoppable.h>


/*!
    \note This class is not thread-safe
*/
namespace nx_http
{
    //!Sync http client. This synchronous wrapper on top of \a AsyncHttpClient
    /*!
        \warning Message body is read ascynhronously to some internal buffer
    */
    class HttpClient
    :
        public QObject,
        public QnStoppable
    {
        Q_OBJECT

    public:
        HttpClient();
        ~HttpClient();

        virtual void pleaseStop() override;

        /*!
            Returns on receiving response
        */
        bool doGet( const QUrl& url );
        bool doPost(
            const QUrl& url,
            const nx_http::StringType& contentType,
            const nx_http::StringType& messageBody );
        const Response* response() const;
        //!
        bool eof() const;
        //!
        /*!
            Blocks, if internal message body buffer is empty
        */
        BufferType fetchMessageBodyBuffer();
        void addAdditionalHeader(const StringType& key, const StringType& value);
        const QUrl& url() const;
        StringType contentType() const;

        //!See \a AsyncHttpClient::setSubsequentReconnectTries
        void setSubsequentReconnectTries( int reconnectTries );
        //!See \a AsyncHttpClient::setTotalReconnectTries
        void setTotalReconnectTries( int reconnectTries );
        //!See \a AsyncHttpClient::setMessageBodyReadTimeoutMs
        void setMessageBodyReadTimeoutMs( unsigned int messageBodyReadTimeoutMs );
        void setUserAgent( const QString& userAgent );
        void setUserName( const QString& userAgent );
        void setUserPassword( const QString& userAgent );

    private:
        nx_http::AsyncHttpClientPtr m_asyncHttpClient;
        QnWaitCondition m_cond;
        mutable QnMutex m_mutex;
        bool m_done;
        bool m_terminated;
        nx_http::BufferType m_msgBodyBuffer;

    private slots:
        void onResponseReceived();
        void onSomeMessageBodyAvailable();
        void onDone();
        void onReconnected();
    };
}

#endif  //HTTPCLIENT_H
