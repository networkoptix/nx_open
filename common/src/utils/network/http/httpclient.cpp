/**********************************************************
* 26 nov 2012
* a.kolesnikov
***********************************************************/

#include "httpclient.h"

#include <QtCore/QMutexLocker>


namespace nx_http
{
    HttpClient::HttpClient()
    :
        m_asyncHttpClient( nx_http::AsyncHttpClient::create() ),
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
        QMutexLocker lk( &m_mutex );
        m_terminated = true;
        m_cond.wakeAll();
    }

    bool HttpClient::doGet( const QUrl& url )
    {
        using namespace std::placeholders;
        return doRequest(std::bind(&nx_http::AsyncHttpClient::doGet, _1, url));
    }

    bool HttpClient::doPost(
        const QUrl& url,
        const nx_http::StringType& contentType,
        nx_http::StringType messageBody )
    {
        using namespace std::placeholders;
        return doRequest(std::bind(
            &nx_http::AsyncHttpClient::doPost,
            _1,
            url,
            contentType,
            std::move(messageBody)));
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
        while( !m_terminated && (m_msgBodyBuffer.isEmpty() && !m_done) )
            m_cond.wait( lk.mutex() );
        nx_http::BufferType result;
        result.swap( m_msgBodyBuffer );
        return result;
    }

    void HttpClient::addAdditionalHeader( const StringType& key, const StringType& value )
    {
        m_additionalHeaders.emplace_back(key, value);
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
        m_subsequentReconnectTries = reconnectTries;
    }

    void HttpClient::setTotalReconnectTries( int reconnectTries )
    {
        m_reconnectTries = reconnectTries;
    }

    void HttpClient::setMessageBodyReadTimeoutMs( unsigned int messageBodyReadTimeoutMs )
    {
        m_messageBodyReadTimeoutMs = messageBodyReadTimeoutMs;
    }

    void HttpClient::setUserAgent( const QString& userAgent )
    {
        m_userAgent = userAgent;
    }

    void HttpClient::setUserName( const QString& userName )
    {
        m_userName = userName;
    }

    void HttpClient::setUserPassword( const QString& userPassword )
    {
        m_userPassword = userPassword;
    }

    template<typename AsyncClientFunc>
        bool HttpClient::doRequest(AsyncClientFunc func)
    {
        if (m_done)
        {
            m_done = false;
        }
        else
        {
            //have to re-establish connection if previous message has not been read up to the end
            if (m_asyncHttpClient)
            {
                m_asyncHttpClient->terminate();
                m_asyncHttpClient.reset();
            }
            m_asyncHttpClient = nx_http::AsyncHttpClient::create();
            
            //TODO #ak setting up attributes
            for (const auto& keyValue: m_additionalHeaders)
                m_asyncHttpClient->addAdditionalHeader(keyValue.first, keyValue.second);
            if (m_subsequentReconnectTries)
                m_asyncHttpClient->setSubsequentReconnectTries(m_subsequentReconnectTries.get());
            if (m_reconnectTries)
                m_asyncHttpClient->setTotalReconnectTries(m_reconnectTries.get());
            if (m_messageBodyReadTimeoutMs)
                m_asyncHttpClient->setMessageBodyReadTimeoutMs(m_messageBodyReadTimeoutMs.get());
            if (m_userAgent)
                m_asyncHttpClient->setUserAgent(m_userAgent.get());
            if (m_userName)
                m_asyncHttpClient->setUserName(m_userName.get());
            if (m_userPassword)
                m_asyncHttpClient->setUserPassword(m_userPassword.get());

            connect(m_asyncHttpClient.get(), &AsyncHttpClient::responseReceived, this, &HttpClient::onResponseReceived, Qt::DirectConnection);
            connect(m_asyncHttpClient.get(), &AsyncHttpClient::someMessageBodyAvailable, this, &HttpClient::onSomeMessageBodyAvailable, Qt::DirectConnection);
            connect(m_asyncHttpClient.get(), &AsyncHttpClient::done, this, &HttpClient::onDone, Qt::DirectConnection);
        }

        if (!func(m_asyncHttpClient.get()))
            return false;

        QMutexLocker lk( &m_mutex );
        while( !m_terminated && (m_asyncHttpClient->state() < AsyncHttpClient::sResponseReceived) )
            m_cond.wait( lk.mutex() );

        return m_asyncHttpClient->state() != AsyncHttpClient::sFailed;
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
