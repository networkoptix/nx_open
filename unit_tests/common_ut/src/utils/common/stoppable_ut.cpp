#include <gtest/gtest.h>
#include <utils/common/stoppable.h>
#include <utils/common/cpp14.h>

struct StoppableTestClass
    : public QnStoppableAsync
{
    StoppableTestClass( int64_t delay )
        : isRunning( true )
    {
        bgThread = std::thread( [ this, delay ]()
        {
            std::this_thread::sleep_for( std::chrono::microseconds( delay ) );
            isRunning = false;
            bgThread.detach();

            if (onStop)
                onStop();
        } );
    }

    ~StoppableTestClass()
    {
    }

    virtual void pleaseStop( std::function< void() > completionHandler ) override
    {
        onStop = std::move( completionHandler );
    }

    std::atomic< bool > isRunning;
    std::thread bgThread;
    std::function< void() > onStop;
};

TEST( QnStoppableAsync, SingleAsync )
{
    StoppableTestClass s( 500 );
    std::promise< bool > p;
    s.pleaseStop([ & ](){ p.set_value( true ); });

    ASSERT_TRUE( s.isRunning );
    ASSERT_TRUE( p.get_future().get() );
    ASSERT_FALSE( s.isRunning );
}

TEST( QnStoppableAsync, SingleSync )
{
    StoppableTestClass s( 500 );
    std::promise< bool > p;

    s.pleaseStopSync();
    ASSERT_FALSE( s.isRunning );
}

TEST( QnStoppableAsync, MultiManual )
{
    StoppableTestClass s1( 500 );
    StoppableTestClass s2( 1000 );
    StoppableTestClass s3( 1500 );

    ASSERT_TRUE( s1.isRunning && s2.isRunning && s3.isRunning );
    s1.pleaseStopSync();
    ASSERT_TRUE( !s1.isRunning && s2.isRunning && s3.isRunning );
    s2.pleaseStopSync();
    ASSERT_TRUE( !s1.isRunning && !s2.isRunning && s3.isRunning );
    s3.pleaseStopSync();
    ASSERT_TRUE( !s1.isRunning && !s2.isRunning && !s3.isRunning );
}

TEST( QnStoppableAsync, MultiAuto )
{
    auto s1 = std::make_unique< StoppableTestClass >( 500 );
    auto s2 = std::make_unique< StoppableTestClass >( 1000 );

    std::promise< bool > p;
    QnStoppableAsync::pleaseStop( [ & ](){ p.set_value( true ); },
                                  std::move( s1 ), std::move( s2 ) );

    ASSERT_TRUE( p.get_future().get() );
}
