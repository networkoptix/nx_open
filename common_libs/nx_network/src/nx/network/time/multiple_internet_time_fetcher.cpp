/**********************************************************
* 5 feb 2015
* a.kolesnikov
***********************************************************/

#include "multiple_internet_time_fetcher.h"

#include <nx/utils/thread/mutex.h>

#include <limits>


MultipleInternetTimeFetcher::TimeFetcherContext::TimeFetcherContext()
:
    utcMillis( -1 ),
    errorCode( SystemError::noError ),
    state( init )
{
}


MultipleInternetTimeFetcher::MultipleInternetTimeFetcher( qint64 maxDeviationMillis )
:
    m_maxDeviationMillis( maxDeviationMillis ),
    m_awaitedAnswers( 0 ),
    m_terminated( false )
{
}

MultipleInternetTimeFetcher::~MultipleInternetTimeFetcher()
{
    pleaseStop();
    join();
}

void MultipleInternetTimeFetcher::pleaseStop()
{
    QnMutexLocker lk( &m_mutex );
    m_terminated = true;
    for( std::unique_ptr<TimeFetcherContext>& ctx: m_timeFetchers )
        ctx->timeFetcher->pleaseStop();
}

void MultipleInternetTimeFetcher::join()
{
    std::vector<std::unique_ptr<TimeFetcherContext>> timeFetchers;
    {
        QnMutexLocker lk( &m_mutex );
        m_timeFetchers.swap( timeFetchers );
    }
    for( std::unique_ptr<TimeFetcherContext>& ctx: timeFetchers )
        ctx->timeFetcher->join();
}

void MultipleInternetTimeFetcher::getTimeAsync( std::function<void(qint64, SystemError::ErrorCode)> handlerFunc )
{
    using namespace std::placeholders;

    QnMutexLocker lk( &m_mutex );

    assert( !m_timeFetchers.empty() );

    for( std::unique_ptr<TimeFetcherContext>& ctx: m_timeFetchers )
    {
        ctx->timeFetcher->getTimeAsync( std::bind(
            &MultipleInternetTimeFetcher::timeFetchingDone,
            this, ctx.get(), _1, _2 ) );
        ++m_awaitedAnswers;
        ctx->errorCode = SystemError::noError;
        ctx->state = TimeFetcherContext::working;
    }

    m_handlerFunc = std::move(handlerFunc);
}

void MultipleInternetTimeFetcher::addTimeFetcher( std::unique_ptr<AbstractAccurateTimeFetcher> timeFetcher )
{
    QnMutexLocker lk( &m_mutex );

    std::unique_ptr<TimeFetcherContext> ctx( new TimeFetcherContext() );
    ctx->timeFetcher = std::move( timeFetcher );
    m_timeFetchers.push_back( std::move(ctx) );
}

void MultipleInternetTimeFetcher::timeFetchingDone(
    TimeFetcherContext* ctx,
    qint64 utcMillis,
    SystemError::ErrorCode errorCode )
{
    QnMutexLocker lk( &m_mutex );

    ctx->utcMillis = utcMillis;
    ctx->errorCode = errorCode;
    ctx->state = TimeFetcherContext::done;

    Q_ASSERT( m_awaitedAnswers > 0 );
    if( (--m_awaitedAnswers) > 0 )
        return;

    if( m_terminated )
        return;

    //all fetchers answered, analyzing results
    qint64 meanUtcTimeMillis = 0;
    qint64 minUtcTimeMillis = std::numeric_limits<qint64>::max();
    size_t collectedValuesCount = 0;
    for( std::unique_ptr<TimeFetcherContext>& ctx: m_timeFetchers )
    {
        if( ctx->state == TimeFetcherContext::init )
            continue;   //this fetcher has been added after getTimeAsync call

        if( ctx->errorCode != SystemError::noError )
        {
            auto handlerFunc = std::move( m_handlerFunc );
            lk.unlock();
            handlerFunc( -1, ctx->errorCode );
            return;
        }

        //analyzing fetched time and calculating mean value
        if( (minUtcTimeMillis != std::numeric_limits<qint64>::max()) &&
            (qAbs(ctx->utcMillis - minUtcTimeMillis) > m_maxDeviationMillis) )
        {
            //failure
            auto handlerFunc = std::move( m_handlerFunc );
            lk.unlock();
            handlerFunc( -1, SystemError::invalidData );
            return;
        }

        if( ctx->utcMillis < minUtcTimeMillis )
            minUtcTimeMillis = ctx->utcMillis;
        meanUtcTimeMillis += ctx->utcMillis;
        ++collectedValuesCount;
    }

    Q_ASSERT( collectedValuesCount > 0 );

    auto handlerFunc = std::move( m_handlerFunc );
    lk.unlock();

    handlerFunc( meanUtcTimeMillis / collectedValuesCount, SystemError::noError );
}
