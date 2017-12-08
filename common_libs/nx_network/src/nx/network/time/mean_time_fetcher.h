#pragma once

#include <memory>
#include <vector>

#include "abstract_accurate_time_fetcher.h"

namespace nx {
namespace network {

/**
 * Fetches time from multiple time fetchers. If all succeeded than returns mean value.
 */
class NX_NETWORK_API MeanTimeFetcher:
    public AbstractAccurateTimeFetcher
{
public:
    static const qint64 kDefaultMaxDeviationMillis = 60*1000;

    /**
     * @param maxDeviationMillis Maximum allowed deviation between time values received via different time fetchers
     */
    MeanTimeFetcher(qint64 maxDeviationMillis = kDefaultMaxDeviationMillis);
    virtual ~MeanTimeFetcher();

    virtual void stopWhileInAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void getTimeAsync(CompletionHandler completionHandler) override;

    void addTimeFetcher(std::unique_ptr<AbstractAccurateTimeFetcher> timeFetcher);

private:
    struct TimeFetcherContext
    {
        std::unique_ptr<AbstractAccurateTimeFetcher> timeFetcher;
        qint64 utcMillis;
        SystemError::ErrorCode errorCode;

        TimeFetcherContext();
    };

    qint64 m_maxDeviationMillis;
    std::vector<std::unique_ptr<TimeFetcherContext>> m_timeFetchers;
    size_t m_awaitedAnswers;
    CompletionHandler m_completionHandler;

    void getTimeAsyncInAioThread(CompletionHandler completionHandler);
    void timeFetchingDone(
        TimeFetcherContext* ctx,
        qint64 utcMillis,
        SystemError::ErrorCode errorCode);
};

} // namespace network
} // namespace nx
