#include <gtest/gtest.h>
#include <utils/common/stoppable.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/std/future.h>


struct StoppableTestClass
    : public QnStoppableAsync
{
public:
    StoppableTestClass()
        : m_isRunning( true )
    {
        m_thread = std::thread( [ this ]()
        {
            nx::utils::MoveOnlyFunc< void() > handler;
            {
                QnMutexLocker lk( &m_mutex );
                while( !m_handler )
                    m_condition.wait( &m_mutex );

                m_isRunning = false;
                handler = std::move( m_handler );
            }

            m_thread.detach();
            handler();
        } );
    }

    bool isRunning() const { return m_isRunning; }

    virtual void pleaseStop( nx::utils::MoveOnlyFunc< void() > completionHandler ) override
    {
        QnMutexLocker lk( &m_mutex );
        m_handler = std::move( completionHandler );
        m_condition.wakeOne();
    }

private:
    QnMutex m_mutex;
    bool m_isRunning;
    QnWaitCondition m_condition;
    nx::utils::MoveOnlyFunc< void() > m_handler;
    std::thread m_thread;
};

TEST( QnStoppableAsync, SingleAsync )
{
    StoppableTestClass s;
    nx::utils::promise< bool > p;
    s.pleaseStop([ & ](){ p.set_value( true ); });
    ASSERT_TRUE( p.get_future().get() );
    ASSERT_FALSE( s.isRunning() );
}

TEST( QnStoppableAsync, SingleSync )
{
    StoppableTestClass s;
    s.pleaseStopSync();
    ASSERT_FALSE( s.isRunning() );
}

TEST( QnStoppableAsync, MultiManual )
{
    StoppableTestClass s1;
    StoppableTestClass s2;
    StoppableTestClass s3;

    auto runningCount = [ & ]() {
        return ( s1.isRunning() ? 1 : 0 ) +
               ( s2.isRunning() ? 1 : 0 ) +
               ( s3.isRunning() ? 1 : 0 );
    };

    EXPECT_EQ( runningCount(), 3 );
    s1.pleaseStopSync();
    EXPECT_EQ( runningCount(), 2 );
    s2.pleaseStopSync();
    EXPECT_EQ( runningCount(), 1 );
    s3.pleaseStopSync();
    EXPECT_EQ( runningCount(), 0 );
}
