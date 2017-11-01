#include <condition_variable>
#include <mutex>
#include <vector>

#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/random.h>

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
    }

    void addGetRequest( const nx::utils::Url& url )
    {
        m_getRequestUrls.push_back( url );
    }

    void addUpdateRequest( const nx::utils::Url& url, const QByteArray& msgBody )
    {
        m_updateRequests.push_back( std::make_pair( url, msgBody ) );
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
            nx_http::AsyncHttpClientPtr newClient = nx_http::AsyncHttpClient::create();
            connect( newClient.get(), &nx_http::AsyncHttpClient::done, 
                     this, &RequestsGenerator::startAnotherClient,
                     Qt::DirectConnection );
            newClient->setResponseReadTimeoutMs( 60*1000 );
            const Request& request = getRequestToPerform();
            if( request.requestType == Request::GET_REQUEST )
                newClient->doGet( request.url );
            else
                newClient->doPost( request.url, "application/json", request.body );
            ++m_requestsStarted;
            m_runningRequests.push_back( std::move(newClient) );
        }

        if( httpClient )
        {
            ++m_requestsCompleted;
            m_runningRequests.erase( std::find(m_runningRequests.begin(), m_runningRequests.end(), httpClient) );
            m_cond.notify_all();
        }
    }

    void start()
    {
        NX_ASSERT( !m_getRequestUrls.empty() || !m_updateRequests.empty() );
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
    class Request
    {
    public:
        static const int GET_REQUEST = 1;
        static const int UPDATE_REQUEST = 2;

        int requestType;
        nx::utils::Url url;
        //!for update requests
        QByteArray body;

        //!create get requests
        Request( const nx::utils::Url& _url )
        :
            requestType( GET_REQUEST ),
            url( _url )
        {
        }

        //!create update requests
        Request( const nx::utils::Url& _url, const QByteArray& _body )
        :
            requestType( UPDATE_REQUEST ),
            url( _url ),
            body( _body )
        {
        }
    };

    std::vector<nx::utils::Url> m_getRequestUrls;
    std::vector<std::pair<nx::utils::Url, QByteArray>> m_updateRequests;
    int m_requestsStarted;
    int m_requestsCompleted;
    int m_maxSimultaneousRequestsCount;
    int m_maxRequestsToPerform;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::list<nx_http::AsyncHttpClientPtr> m_runningRequests;

    Request getRequestToPerform() const
    {
        if( !m_updateRequests.empty() && nx::utils::random::number(0, 1) )
        {
            const auto& request = nx::utils::random::choice( m_updateRequests );
            return Request( request.first, request.second );
        }
        else
        {
            return Request( nx::utils::random::choice( m_getRequestUrls ) );
        }
    }
};

//TODO #ak same class
class Ec2APITest
:
    public ::testing::Test
{
protected:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }
};

#if 0
TEST_F( Ec2APITest, randomGet )
{
    QUrl requestUrl( lit("http://127.0.0.1:7001/") );
    //QUrl requestUrl( lit("http://192.168.0.71:7001/") );
    requestUrl.setUserName( lit("admin") );
    requestUrl.setPassword( lit("123") );

    RequestsGenerator reqGen;
    requestUrl.setPath( lit("/ec2/getMediaServersEx") );
    reqGen.addGetRequest( requestUrl );
    //requestUrl.setPath( lit("/ec2/getLicenses") );
    //reqGen.addGetRequest( requestUrl );

#if 0
    requestUrl.setPath( lit("/ec2/saveCameras") );
    QByteArray saveCameraJson =
        "[\n"
        "    {\n"
        "        \"groupId\": \"\",\n"
        "        \"groupName\": \"\",\n"
        "        \"id\": \"{594caa1a-ae53-380c-2022-b264141d2ffd}\",\n"
        "        \"mac\": \"00-1C-A6-01-21-A1\",\n"
        "        \"manuallyAdded\": false,\n"
        "        \"model\": \"DWC-MD421D\",\n"
        "        \"name\": \"DWC-MD421D test\",\n"
        "        \"parentId\": \"{47bf37a0-72a6-2890-b967-5da9c390d28a}\",\n"
        "        \"physicalId\": \"00-1C-A6-01-21-A1\",\n"
        "        \"statusFlags\": \"CSF_NoFlags\",\n"
        "        \"typeId\": \"{eb1e8a6a-da43-5c75-c08f-72f94205e0d9}\",\n"
        "        \"url\": \"192.168.0.91\",\n"
        "        \"vendor\": \"Digital Watchdog\"\n"
        "    }\n"
        "]\n";
    reqGen.addUpdateRequest( requestUrl, saveCameraJson );
#endif

    reqGen.setMaxRequestsToPerform( 70000 );
    reqGen.setMaxSimultaneousRequestsCount( 100 );
    reqGen.start();
    reqGen.wait();
}
#endif

#if 0
TEST_F( Ec2APITest, removeResource )
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
