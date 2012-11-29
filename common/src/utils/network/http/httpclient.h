/**********************************************************
* 26 nov 2012
* a.kolesnikov
***********************************************************/

#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include "asynchttpclient.h"

#include <QWaitCondition>
#include <QMutex>


/*!
    \note This class is not thread-safe
*/
namespace nx_http
{
    //!Sync http client
    class HttpClient
    :
        public QObject
    {
        Q_OBJECT

    public:
        HttpClient();

        /*!
            Returns on receiving response
        */
        bool doGet( const QUrl& url );
        const HttpResponse* response() const;
        //!
        bool eof() const;
        //!
        /*!
            Blocks, if internal message body buffer is empty
        */
        BufferType fetchMessageBodyBuffer();
        const QUrl& url() const;
        bool startReadMessageBody();

        void setSubsequentReconnectTries( int reconnectTries );
        void setTotalReconnectTries( int reconnectTries );
        void setUserAgent( const QString& userAgent );
        void setUserName( const QString& userAgent );
        void setUserPassword( const QString& userAgent );

    private:
        AsyncHttpClient m_asyncHttpClient;
        QWaitCondition m_cond;
        mutable QMutex m_mutex;
        bool m_done;
        nx_http::BufferType m_msgBodyBuffer;

    private slots:
        void onResponseReceived();
        void onSomeMessageBodyAvailable();
        void onDone();
        void onReconnected();
    };
}

#endif  //HTTPCLIENT_H
