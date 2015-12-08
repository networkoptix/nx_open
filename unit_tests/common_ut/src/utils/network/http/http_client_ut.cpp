/**********************************************************
* 26 dec 2014
* a.kolesnikov
***********************************************************/

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#define QN_NO_KEYWORD_UNUSED
#include <gtest/gtest.h>

#include <common/common_globals.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/network/http/httpclient.h>


//TODO #ak introduce built-in http server to automate AsyncHttpClient tests

TEST( HttpClient, DISABLED_KeepAlive )
{
    //TODO #ak use local http server

    static const QUrl url( "http://192.168.0.1/girls/candice_swanepoel_07_original.jpg" );
    static const int TEST_RUNS = 2;

    nx_http::HttpClient client;

    nx::Buffer msgBody;
    for( int i = 0; i < TEST_RUNS; ++i )
    {
        ASSERT_TRUE( client.doGet( url ) );
        ASSERT_TRUE( client.response() );
        ASSERT_EQ( client.response()->statusLine.statusCode, nx_http::StatusCode::ok );
        nx::Buffer newMsgBody;
        while( !client.eof() )
            newMsgBody += client.fetchMessageBodyBuffer();
        if( i == 0 )
            msgBody = newMsgBody;
        else
            ASSERT_EQ( msgBody, newMsgBody );
    }
}

TEST( HttpClient, DISABLED_KeepAlive2 )
{
    QUrl url( "http://192.168.0.1:7001/ec2/testConnection" );
    url.setUserName( "admin" );
    url.setPassword( "123" );
    static const int TEST_RUNS = 2;

    nx::Buffer msgBody;
    for( int i = 0; i < TEST_RUNS; ++i )
    {
        nx_http::HttpClient client;
        ASSERT_TRUE( client.doGet( url ) );
        ASSERT_TRUE( client.response() );
        ASSERT_EQ( nx_http::StatusCode::ok, client.response()->statusLine.statusCode );
        nx::Buffer newMsgBody;
        while( !client.eof() )
            newMsgBody += client.fetchMessageBodyBuffer();
        if( i == 0 )
            msgBody = newMsgBody;
        else
            ASSERT_EQ( msgBody, newMsgBody );
    }
}

TEST( HttpClient, DISABLED_KeepAlive3 )
{
    QUrl url( "http://192.168.0.194:7001/ec2/events?guid=%7Be7209f3e-9ebe-6ebb-3e99-e5acd61c228c%7D&runtime-guid=%7B83862a97-b7b8-4dbc-bb8f-64847f23e6d5%7D&system-identity-time=0" );
    url.setUserName( "admin" );
    url.setPassword( "123" );
    static const int TEST_RUNS = 2;

    nx::Buffer msgBody;
    for( int i = 0; i < TEST_RUNS; ++i )
    {
        nx_http::HttpClient client;
        client.addAdditionalHeader( "NX-EC-SYSTEM-NAME", "ak_ec_2.3" );
        ASSERT_TRUE( client.doGet( url ) );
        ASSERT_TRUE( client.response() );
        ASSERT_EQ( nx_http::StatusCode::ok, client.response()->statusLine.statusCode );
        nx::Buffer newMsgBody;
        while( !client.eof() )
            newMsgBody += client.fetchMessageBodyBuffer();
        if( i == 0 )
            msgBody = newMsgBody;
        else
            ASSERT_EQ( msgBody, newMsgBody );
    }
}

TEST(HttpClient, DISABLED_mjpgRetrieval)
{
    QUrl url("http://192.168.0.92/mjpg/video.mjpg");
    url.setUserName("root");
    url.setPassword("root");
    static const int TEST_RUNS = 100;

    nx::Buffer msgBody;
    nx_http::HttpClient client;
    for (int i = 0; i < TEST_RUNS; ++i)
    {
        ASSERT_TRUE(client.doGet(url));
        ASSERT_TRUE(client.response());
        ASSERT_EQ(nx_http::StatusCode::ok, client.response()->statusLine.statusCode);
        nx::Buffer newMsgBody;
        const int readsCount = (rand() % 10) + 10;
        for (int i = 0; i < readsCount; ++i)
            newMsgBody += client.fetchMessageBodyBuffer();
        continue;
    }
}

TEST(HttpClient, DISABLED_fileDownload2)
{
    const QUrl url("http://192.168.0.1/girls/Candice-Swanepoel-127.jpg");
    static const int THREADS = 10;
    static const std::chrono::seconds TEST_DURATION(15);

    std::vector<std::thread> threads;
    std::vector<std::pair<bool, std::shared_ptr<nx_http::HttpClient>>> clients;
    std::mutex mtx;
    std::atomic<bool> isTerminated(false);

    auto threadFunc = [url, &clients, &mtx, &isTerminated]()
    {
        while (!isTerminated)
        {
            nx::Buffer msgBody;

            std::unique_lock<std::mutex> lk(mtx);
            int pos = rand() % clients.size();
            while (clients[pos].first)
                pos = (pos+1) % clients.size();
            auto client = clients[pos].second;
            clients[pos].first = true;
            lk.unlock();

            ASSERT_TRUE(client->doGet(url));
            if ((rand() % 3 != 0) && !client->eof() && (client->response() != nullptr))
            {
                const auto statusCode = client->response()->statusLine.statusCode;
                ASSERT_EQ(nx_http::StatusCode::ok, statusCode);
                nx::Buffer newMsgBody;
                while (!client->eof())
                    newMsgBody += client->fetchMessageBodyBuffer();
            }

            lk.lock();
            if (clients[pos].second == client)
                clients[pos].first = false;
        }
    };

    for (int i = 0; i < THREADS; ++i)
        clients.emplace_back(false, std::make_shared<nx_http::HttpClient>());

    for (int i = 0; i < THREADS; ++i)
        threads.emplace_back(std::thread(threadFunc));

    const auto beginTime = std::chrono::steady_clock::now();
    while (beginTime + TEST_DURATION > std::chrono::steady_clock::now())
    {
        std::unique_lock<std::mutex> lk(mtx);
        const int pos = rand() % clients.size();
        auto client = clients[pos].second;
        lk.unlock();
        
        client->pleaseStop();
        client.reset();
        
        lk.lock();
        clients[pos].second = std::make_shared<nx_http::HttpClient>();
        clients[pos].first = false;
        lk.unlock();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    isTerminated = true;

    for (int i = 0; i < THREADS; ++i)
        threads[i].join();
}
