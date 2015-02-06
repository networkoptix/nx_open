/**********************************************************
* 6 feb 2015
* a.kolesnikov
***********************************************************/

#include <chrono>
#include <condition_variable>
#include <mutex>

#include <gtest/gtest.h>

#include <utils/network/time/multiple_internet_time_fetcher.h>
#include <utils/network/time_protocol_client.h>


static const char* NIST_RFC868_SERVER = "time.nist.gov";
static const char* UCLA_RFC868_SERVER = "time1.ucla.edu";

TEST( InternetTimeFetcher, genericTest )
{
    std::unique_ptr<MultipleInternetTimeFetcher> timeFetcher;
    timeFetcher.reset( new MultipleInternetTimeFetcher() );

    timeFetcher->addTimeFetcher( std::unique_ptr<AbstractAccurateTimeFetcher>(
        new TimeProtocolClient(QLatin1String(NIST_RFC868_SERVER))) );
    timeFetcher->addTimeFetcher( std::unique_ptr<AbstractAccurateTimeFetcher>(
        new TimeProtocolClient(QLatin1String(UCLA_RFC868_SERVER))) );

    std::condition_variable condVar;
    std::mutex mutex;
    bool done = false;
    std::vector<qint64> utcMillis;

    const auto timerStart = std::chrono::steady_clock::now();

    for( int i = 0; i < 10; ++i )
    {
        const bool result = timeFetcher->getTimeAsync(
            [&condVar, &mutex, &done, &utcMillis]( qint64 _utcMillis, SystemError::ErrorCode errorCode ) {
                std::unique_lock<std::mutex> lk( mutex );
                if( errorCode == SystemError::noError )
                    utcMillis.push_back(_utcMillis);
                done = true;
                condVar.notify_all();
            } );
        ASSERT_TRUE( result );

        std::unique_lock<std::mutex> lk( mutex );
        condVar.wait( lk, [&done]()->bool{ return done; } );
        done = false;
    }

    ASSERT_GT( utcMillis.size(), 0 );

    const auto elapsedMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - timerStart);

    qint64 minTimestamp = std::numeric_limits<qint64>::max();
    for( qint64 ts: utcMillis )
    {
        if( ts < minTimestamp )
            minTimestamp = ts;
        ASSERT_TRUE( abs(ts - minTimestamp) < (elapsedMillis.count() * 2) );
    }
}
