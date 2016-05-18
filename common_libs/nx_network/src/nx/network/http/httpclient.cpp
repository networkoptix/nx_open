/**********************************************************
* 26 nov 2012
* a.kolesnikov
***********************************************************/

#include "httpclient.h"

#include <nx/utils/thread/mutex.h>


namespace nx_http
{
    HttpClient::HttpClient()
    :
        m_done( false ),
        m_terminated( false )
    {
        instanciateHttpClient();
    }

    HttpClient::~HttpClient()
    {
        pleaseStop();
        m_asyncHttpClient->terminate();
    }

    void HttpClient::pleaseStop()
    {
        QnMutexLocker lk( &m_mutex );
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

    SystemError::ErrorCode HttpClient::lastSysErrorCode() const
    {
        return m_asyncHttpClient->lastSysErrorCode();
    }

    //!
    bool HttpClient::eof() const
    {
        QnMutexLocker lk( &m_mutex );
        return m_done && m_msgBodyBuffer.isEmpty();
    }

    //!
    /*!
        Blocks, if internal message body buffer is empty
    */
    BufferType HttpClient::fetchMessageBodyBuffer()
    {
        QnMutexLocker lk( &m_mutex );
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

    void HttpClient::setSendTimeoutMs( unsigned int sendTimeoutMs )
    {
        m_sendTimeoutMs = sendTimeoutMs;
    }

    void HttpClient::setResponseReadTimeoutMs(unsigned int responseReadTimeoutMs)
    {
        m_responseReadTimeoutMs = responseReadTimeoutMs;
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

    AbstractStreamSocket* HttpClient::socket()
    {
        return m_asyncHttpClient->socket();
    }

    QSharedPointer<AbstractStreamSocket> HttpClient::takeSocket()
    {
        return m_asyncHttpClient->takeSocket();
    }

    void HttpClient::instanciateHttpClient()
    {
        m_asyncHttpClient = nx_http::AsyncHttpClient::create();
        connect(m_asyncHttpClient.get(), &AsyncHttpClient::responseReceived, this, &HttpClient::onResponseReceived, Qt::DirectConnection);
        connect(m_asyncHttpClient.get(), &AsyncHttpClient::someMessageBodyAvailable, this, &HttpClient::onSomeMessageBodyAvailable, Qt::DirectConnection);
        connect(m_asyncHttpClient.get(), &AsyncHttpClient::done, this, &HttpClient::onDone, Qt::DirectConnection);
    }

    template<typename AsyncClientFunc>
        bool HttpClient::doRequest(AsyncClientFunc func)
    {
        QnMutexLocker lk(&m_mutex);

        if (!m_done)
        {
            lk.unlock();

            //have to re-establish connection if previous message has not been read up to the end
            if (m_asyncHttpClient)
            {
                m_asyncHttpClient->terminate();
                m_asyncHttpClient.reset();
            }
            instanciateHttpClient();

            //setting up attributes
            for (const auto& keyValue: m_additionalHeaders)
                m_asyncHttpClient->addAdditionalHeader(keyValue.first, keyValue.second);
            if (m_subsequentReconnectTries)
                m_asyncHttpClient->setSubsequentReconnectTries(m_subsequentReconnectTries.get());
            if (m_reconnectTries)
                m_asyncHttpClient->setTotalReconnectTries(m_reconnectTries.get());
            if (m_sendTimeoutMs)
                m_asyncHttpClient->setSendTimeoutMs(m_sendTimeoutMs.get());
            if (m_responseReadTimeoutMs)
                m_asyncHttpClient->setResponseReadTimeoutMs(m_responseReadTimeoutMs.get());
            if (m_messageBodyReadTimeoutMs)
                m_asyncHttpClient->setMessageBodyReadTimeoutMs(m_messageBodyReadTimeoutMs.get());
            if (m_userAgent)
                m_asyncHttpClient->setUserAgent(m_userAgent.get());
            if (m_userName)
                m_asyncHttpClient->setUserName(m_userName.get());
            if (m_userPassword)
                m_asyncHttpClient->setUserPassword(m_userPassword.get());

            lk.relock();
        }

        m_done = false;
        func(m_asyncHttpClient.get());

        m_msgBodyBuffer.clear();
        while (!m_terminated && (m_asyncHttpClient->state() < AsyncHttpClient::sResponseReceived))
            m_cond.wait(lk.mutex());

        return m_asyncHttpClient->state() != AsyncHttpClient::sFailed;
    }

    void HttpClient::onResponseReceived()
    {
        QnMutexLocker lk( &m_mutex );
        //message body buffer can be non-empty
        m_msgBodyBuffer += m_asyncHttpClient->fetchMessageBodyBuffer();
        m_cond.wakeAll();
    }

    void HttpClient::onSomeMessageBodyAvailable()
    {
        QnMutexLocker lk( &m_mutex );
        m_msgBodyBuffer += m_asyncHttpClient->fetchMessageBodyBuffer();
        m_cond.wakeAll();
    }

    void HttpClient::onDone()
    {
        QnMutexLocker lk( &m_mutex );
        m_done = true;
        m_cond.wakeAll();
    }

    void HttpClient::onReconnected()
    {
        //TODO/IMPL
    }
}
