/**********************************************************
* 26 dec 2014
* a.kolesnikov
***********************************************************/

#include <condition_variable>
#include <mutex>

#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/network/http/httpclient.h>


class RequestsGenerator
:
    public QObject
{
public:
    RequestsGenerator()
    :
        m_requestsStarted( 0 ),
        m_requestsCompleted( 0 ),
        m_maxSimultaneousRequestsCount( 100 ),
        m_maxRequestsToPerform( 1000*1000 )
    {
        m_requestUrl = QUrl( lit("http://127.0.0.1:7001/ec2/getCamerasEx") );
        m_requestUrl.setUserName( lit("admin") );
        m_requestUrl.setPassword( lit("123") );
    }

    void setMaxRequestsToPerform( int maxRequestsToPerform )
    {
        m_maxRequestsToPerform = maxRequestsToPerform;
    }

    void setMaxSimultaneousRequestsCount( int maxSimultaneousRequestsCount )
    {
        m_maxSimultaneousRequestsCount = maxSimultaneousRequestsCount;
    }

    void startAnotherClient( nx_http::AsyncHttpClientPtr httpClient )
    {
        std::unique_lock<std::mutex> lk( m_mutex );

        if( m_requestsStarted < maxRequestsToPerform() )
        {
            nx_http::AsyncHttpClientPtr newClient = std::make_shared<nx_http::AsyncHttpClient>();
            connect( newClient.get(), &nx_http::AsyncHttpClient::done, 
                     this, &RequestsGenerator::startAnotherClient,
                     Qt::DirectConnection );
            if( newClient->doGet( getUrl() ) )
            {
                ++m_requestsStarted;
                m_runningRequests.push_back( std::move(newClient) );
            }
        }

        if( httpClient )
        {
            ++m_requestsCompleted;
            m_runningRequests.erase( std::find(m_runningRequests.begin(), m_runningRequests.end(), httpClient) );
            m_cond.notify_all();
        }
    }

    QUrl getUrl() const
    {
        return m_requestUrl;
    }

    void start()
    {
        for( int i = 0; i < m_maxSimultaneousRequestsCount; ++i )
            startAnotherClient( nx_http::AsyncHttpClientPtr() );
    }

    void wait()
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        while( m_requestsCompleted < maxRequestsToPerform() )
            m_cond.wait( lk );
    }

    int maxSimultaneousRequestsCount() const
    {
        return m_maxSimultaneousRequestsCount;
    }

    int maxRequestsToPerform() const
    {
        return m_maxRequestsToPerform;
    }

private:
    QUrl m_requestUrl;
    int m_requestsStarted;
    int m_requestsCompleted;
    int m_maxSimultaneousRequestsCount;
    int m_maxRequestsToPerform;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::list<nx_http::AsyncHttpClientPtr> m_runningRequests;
};

#if 0
TEST( Ec2APITest, randomGet )
{
    RequestsGenerator reqGen;
    reqGen.setMaxRequestsToPerform( 10*1000 );
    reqGen.setMaxSimultaneousRequestsCount( 100 );
    reqGen.start();
    reqGen.wait();
}
#endif

#if 0
TEST( Ec2APITest, removeResource )
{
    //QUrl url( lit("http://192.168.0.58:7001/ec2/removeResource") );
    //QUrl url( lit("http://127.0.0.1:7001/ec2/removeResource") );
    QUrl url( lit("http://192.168.0.25:7001/ec2/removeResource") );
    url.setUserName( lit("admin") );
    url.setPassword( lit("123") );

    QByteArray data = 
        "{"
        "    \"id\": \"{d41d8cd9-8f00-b204-e980-0998ecf8427e}\""
        "}";

    for( int i = 0; i < 1; ++i )
    {
        nx_http::HttpClient httpClient;
        if( !httpClient.doPost( url, "application/json", data ) )
            return 1;

        int statusCode = httpClient.response()->statusLine.statusCode;
        if( statusCode != nx_http::StatusCode::ok )
            int x = 0;
    }

    return 0;
}
#endif
