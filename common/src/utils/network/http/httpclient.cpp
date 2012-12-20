/**********************************************************
* 26 nov 2012
* a.kolesnikov
***********************************************************/

#include "httpclient.h"

#include <QMutexLocker>


namespace nx_http
{
    HttpClient::HttpClient()
    :
        m_asyncHttpClient( new AsyncHttpClient() ),
        m_done( false )
    {
        connect( m_asyncHttpClient, SIGNAL(responseReceived(nx_http::AsyncHttpClient*)), this, SLOT(onResponseReceived()), Qt::DirectConnection );
        connect( m_asyncHttpClient, SIGNAL(someMessageBodyAvailable(nx_http::AsyncHttpClient*)), this, SLOT(onSomeMessageBodyAvailable()), Qt::DirectConnection );
        connect( m_asyncHttpClient, SIGNAL(done(nx_http::AsyncHttpClient*)), this, SLOT(onDone()), Qt::DirectConnection );
    }

    HttpClient::~HttpClient()
    {
        m_asyncHttpClient->scheduleForRemoval();
    }

    bool HttpClient::doGet( const QUrl& url )
    {
        if( !m_asyncHttpClient->doGet( url ) )
            return false;

        QMutexLocker lk( &m_mutex );
        while( m_asyncHttpClient->state() < AsyncHttpClient::sResponseReceived )
            m_cond.wait( lk.mutex() );

        return m_asyncHttpClient->state() != AsyncHttpClient::sFailed;
    }

    const Response* HttpClient::response() const
    {
        return m_asyncHttpClient->response();
    }

    //!
    bool HttpClient::eof() const
    {
        QMutexLocker lk( &m_mutex );
        return m_done && m_msgBodyBuffer.isEmpty();
    }

    //!
    /*!
        Blocks, if internal message body buffer is empty
    */
    BufferType HttpClient::fetchMessageBodyBuffer()
    {
        QMutexLocker lk( &m_mutex );
        while( m_msgBodyBuffer.isEmpty() && !m_done )
            m_cond.wait( lk.mutex() );
        nx_http::BufferType result = m_msgBodyBuffer;
        m_msgBodyBuffer.clear();
        return result;
    }

    const QUrl& HttpClient::url() const
    {
        return m_asyncHttpClient->url();
    }

    bool HttpClient::startReadMessageBody()
    {
        return m_asyncHttpClient->startReadMessageBody();
    }

    void HttpClient::setSubsequentReconnectTries( int reconnectTries )
    {
        m_asyncHttpClient->setSubsequentReconnectTries( reconnectTries );
    }

    void HttpClient::setTotalReconnectTries( int reconnectTries )
    {
        m_asyncHttpClient->setTotalReconnectTries( reconnectTries );
    }

    void HttpClient::setUserAgent( const QString& userAgent )
    {
        m_asyncHttpClient->setUserAgent( userAgent );
    }

    void HttpClient::setUserName( const QString& userName )
    {
        m_asyncHttpClient->setUserName( userName );
    }

    void HttpClient::setUserPassword( const QString& userPassword )
    {
        m_asyncHttpClient->setUserPassword( userPassword );
    }

    void HttpClient::onResponseReceived()
    {
        QMutexLocker lk( &m_mutex );
        //message body buffer can be non-empty
        m_msgBodyBuffer += m_asyncHttpClient->fetchMessageBodyBuffer();
        m_cond.wakeAll();
    }

    void HttpClient::onSomeMessageBodyAvailable()
    {
        QMutexLocker lk( &m_mutex );
        m_msgBodyBuffer += m_asyncHttpClient->fetchMessageBodyBuffer();
        m_cond.wakeAll();
    }

    void HttpClient::onDone()
    {
        QMutexLocker lk( &m_mutex );
        m_done = true;
        m_cond.wakeAll();
    }

    void HttpClient::onReconnected()
    {
        //TODO/IMPL
    }
}
