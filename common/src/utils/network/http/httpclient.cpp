/**********************************************************
* 26 nov 2012
* a.kolesnikov
***********************************************************/

#include "httpclient.h"

#include <utils/common/mutex.h>


namespace nx_http
{
    HttpClient::HttpClient()
    :
        m_asyncHttpClient( std::make_shared<AsyncHttpClient>() ),
        m_done( false ),
        m_terminated( false )
    {
        connect( m_asyncHttpClient.get(), &AsyncHttpClient::responseReceived, this, &HttpClient::onResponseReceived, Qt::DirectConnection );
        connect( m_asyncHttpClient.get(), &AsyncHttpClient::someMessageBodyAvailable, this, &HttpClient::onSomeMessageBodyAvailable, Qt::DirectConnection );
        connect( m_asyncHttpClient.get(), &AsyncHttpClient::done, this, &HttpClient::onDone, Qt::DirectConnection );
    }

    HttpClient::~HttpClient()
    {
        pleaseStop();
        m_asyncHttpClient->terminate();
    }

    void HttpClient::pleaseStop()
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        m_terminated = true;
        m_cond.wakeAll();
    }

    bool HttpClient::doGet( const QUrl& url )
    {
        m_done = false;

        if( !m_asyncHttpClient->doGet( url ) )
            return false;

        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        while( !m_terminated && (m_asyncHttpClient->state() < AsyncHttpClient::sResponseReceived) )
            m_cond.wait( lk.mutex() );

        return m_asyncHttpClient->state() != AsyncHttpClient::sFailed;
    }

    bool HttpClient::doPost(
        const QUrl& url,
        const nx_http::StringType& contentType,
        const nx_http::StringType& messageBody )
    {
        m_done = false;

        if( !m_asyncHttpClient->doPost( url, contentType, messageBody ) )
            return false;

        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
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
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        return m_done && m_msgBodyBuffer.isEmpty();
    }

    //!
    /*!
        Blocks, if internal message body buffer is empty
    */
    BufferType HttpClient::fetchMessageBodyBuffer()
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        while( !m_terminated && (m_msgBodyBuffer.isEmpty() && !m_done) )
            m_cond.wait( lk.mutex() );
        nx_http::BufferType result = m_msgBodyBuffer;
        m_msgBodyBuffer.clear();
        return result;
    }

    void HttpClient::addRequestHeader( const StringType& key, const StringType& value )
    {
        m_asyncHttpClient->addRequestHeader( key, value );
    }

    const QUrl& HttpClient::url() const
    {
        return m_asyncHttpClient->url();
    }

    StringType HttpClient::contentType() const
    {
        return m_asyncHttpClient->contentType();
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
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        //message body buffer can be non-empty
        m_msgBodyBuffer += m_asyncHttpClient->fetchMessageBodyBuffer();
        m_cond.wakeAll();
    }

    void HttpClient::onSomeMessageBodyAvailable()
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        m_msgBodyBuffer += m_asyncHttpClient->fetchMessageBodyBuffer();
        m_cond.wakeAll();
    }

    void HttpClient::onDone()
    {
        SCOPED_MUTEX_LOCK( lk,  &m_mutex );
        m_done = true;
        m_cond.wakeAll();
    }

    void HttpClient::onReconnected()
    {
        //TODO/IMPL
    }
}
