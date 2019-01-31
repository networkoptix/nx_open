#include "mean_time_fetcher.h"

#include <limits>

#include <nx/utils/thread/mutex.h>

namespace nx {
namespace network {

//-------------------------------------------------------------------------------------------------
// MeanTimeFetcher::TimeFetcherContext

MeanTimeFetcher::TimeFetcherContext::TimeFetcherContext():
    utcMillis(-1),
    errorCode(SystemError::noError),
    rtt(std::chrono::milliseconds::zero())
{
}

//-------------------------------------------------------------------------------------------------
// MeanTimeFetcher

MeanTimeFetcher::MeanTimeFetcher(qint64 maxDeviationMillis):
    m_maxDeviationMillis(maxDeviationMillis),
    m_awaitedAnswers(0)
{
}

MeanTimeFetcher::~MeanTimeFetcher()
{
    stopWhileInAioThread();
}

void MeanTimeFetcher::stopWhileInAioThread()
{
    m_timeFetchers.clear();
}

void MeanTimeFetcher::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    AbstractAccurateTimeFetcher::bindToAioThread(aioThread);
    for (auto& timeFetcherContext: m_timeFetchers)
        timeFetcherContext->timeFetcher->bindToAioThread(aioThread);
}

void MeanTimeFetcher::getTimeAsync(
    CompletionHandler completionHandler)
{
    dispatch(
        [this, completionHandler = std::move(completionHandler)]() mutable
        {
            getTimeAsyncInAioThread(std::move(completionHandler));
        });
}

void MeanTimeFetcher::getTimeAsyncInAioThread(
    CompletionHandler completionHandler)
{
    using namespace std::placeholders;

    NX_CRITICAL(!m_timeFetchers.empty());

    for (std::unique_ptr<TimeFetcherContext>& ctx: m_timeFetchers)
    {
        ctx->errorCode = SystemError::noError;
        ctx->utcMillis = -1;
        ctx->timeFetcher->getTimeAsync(std::bind(
            &MeanTimeFetcher::timeFetchingDone,
            this, ctx.get(), _1, _2, _3));
        ++m_awaitedAnswers;
    }

    m_completionHandler = std::move(completionHandler);
}

void MeanTimeFetcher::addTimeFetcher(
    std::unique_ptr<AbstractAccurateTimeFetcher> timeFetcher)
{
    timeFetcher->bindToAioThread(getAioThread());

    std::unique_ptr<TimeFetcherContext> ctx(new TimeFetcherContext());
    ctx->timeFetcher = std::move(timeFetcher);
    m_timeFetchers.push_back(std::move(ctx));
}

void MeanTimeFetcher::timeFetchingDone(
    TimeFetcherContext* ctx,
    qint64 utcMillis,
    SystemError::ErrorCode errorCode,
    std::chrono::milliseconds rtt)
{
    ctx->utcMillis = utcMillis;
    ctx->rtt = rtt;
    ctx->errorCode = errorCode;

    NX_ASSERT(m_awaitedAnswers > 0);
    if ((--m_awaitedAnswers) > 0)
        return;

    //all fetchers answered, analyzing results
    qint64 sumOfReceivedUtcTimesMillis = 0;
    std::chrono::milliseconds sumOfReceivedRtt = std::chrono::milliseconds::zero();
    qint64 minUtcTimeMillis = std::numeric_limits<qint64>::max();
    size_t collectedValuesCount = 0;
    for (std::unique_ptr<TimeFetcherContext>& ctx: m_timeFetchers)
    {
        if (ctx->errorCode != SystemError::noError)
        {
            nx::utils::swapAndCall(m_completionHandler, -1, ctx->errorCode, std::chrono::milliseconds::zero());
            return;
        }

        //analyzing fetched time and calculating mean value
        if ((minUtcTimeMillis != std::numeric_limits<qint64>::max()) &&
            (qAbs(ctx->utcMillis - minUtcTimeMillis) > m_maxDeviationMillis))
        {
            nx::utils::swapAndCall(m_completionHandler, -1, SystemError::invalidData, std::chrono::milliseconds::zero());
            return;
        }

        if (ctx->utcMillis < minUtcTimeMillis)
            minUtcTimeMillis = ctx->utcMillis;
        sumOfReceivedUtcTimesMillis += ctx->utcMillis - ctx->rtt.count() / 2;
        sumOfReceivedRtt += ctx->rtt;
        ++collectedValuesCount;
    }

    NX_ASSERT(collectedValuesCount > 0);

    nx::utils::swapAndCall(
        m_completionHandler,
        sumOfReceivedUtcTimesMillis / collectedValuesCount,
        SystemError::noError,
        sumOfReceivedRtt / collectedValuesCount);
}

} // namespace network
} // namespace nx
