/**********************************************************
* 6 feb 2015
* a.kolesnikov
***********************************************************/

#include <condition_variable>
#include <mutex>

#include <gtest/gtest.h>

#include <QtCore/QElapsedTimer>

#include <utils/network/time/multiple_internet_time_fetcher.h>
#include <utils/network/time/time_protocol_client.h>


static const char* RFC868_SERVERS[] = { "time.nist.gov", "time.ien.it"/*, "time1.ucla.edu"*/ };

#if 0
TEST( InternetTimeFetcher, genericTest )
{
    std::unique_ptr<MultipleInternetTimeFetcher> timeFetcher;
    timeFetcher.reset( new MultipleInternetTimeFetcher() );

    for (const char* timeServer : RFC868_SERVERS)
        timeFetcher->addTimeFetcher(std::unique_ptr<AbstractAccurateTimeFetcher>(
            new TimeProtocolClient(QLatin1String(timeServer))));

    std::condition_variable condVar;
    std::mutex mutex;
    bool done = false;
    std::vector<qint64> utcMillis;

    QElapsedTimer et;
    et.restart();

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

    qint64 minTimestamp = std::numeric_limits<qint64>::max();
    for( qint64 ts: utcMillis )
    {
        if( ts < minTimestamp )
            minTimestamp = ts;
        ASSERT_TRUE( abs(ts - minTimestamp) < (et.elapsed() * 2) );
    }
}
#endif
