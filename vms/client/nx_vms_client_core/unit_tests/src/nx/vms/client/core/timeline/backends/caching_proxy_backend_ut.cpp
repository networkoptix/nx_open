// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nx/vms/client/core/timeline/backends/caching_proxy_backend.h>
#include <utils/common/synctime.h>

#include "test_backends.h"

// Most of the caching behavior tests are implemented in `data_lists_cache_ut.cpp`.
// The purpose of this suite is to ensure the interaction with the source backend is correct.

// clang-format off

namespace nx::vms::client::core::timeline {
namespace test {

using namespace std::chrono;

template<typename SourceBackend>
class TimelineCachingProxyBackendTest: public ::testing::Test
{
    using base_type = ::testing::Test;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        source = std::make_shared<SourceBackend>(TestDataList{
            {6ms, 'A'},
            {6ms, 'B'},
            {6ms, 'C'},

            {5ms, 'D'},
            {5ms, 'E'},

            {4ms, 'F'},
            {4ms, 'G'},

            {3ms, 'H'},
            {3ms, 'I'},
            {3ms, 'J'},

            {2ms, 'K'},

            {1ms, 'L'},
            {1ms, 'M'}});

        backend = std::make_shared<CachingProxyBackend<TestDataList>>(source, &timeSource);
        backend->setAgeThreshold(std::nullopt);
    }

    virtual void TearDown() override
    {
        backend.reset();
        source.reset();
        base_type::TearDown();
    }

    void givenCached(const QnTimePeriod& period, int limit)
    {
        waitForCompletion(backend->load(period, limit));
    }

    void givenExpirationTime(seconds value) { backend->setExpirationTime(value); }
    void givenAgeThreshold(seconds value) { backend->setAgeThreshold(value); }
    void givenSourceFailing() { source->setFailureMode(true); }

    /** A time point the specified age ago. */
    milliseconds agedTimePoint(seconds age) const { return timeSource.value() - age; }

    void whenLoaded(const QnTimePeriod& period, int limit)
    {
        source->log().clear();
        future = backend->load(period, limit);
        waitForCompletion(future);
    }

    void whenInvalidated(const QnTimePeriod& period) { backend->invalidate(period); }
    void whenSourceRecovered() { source->setFailureMode(false); }

    void thenLoadResultIs(const std::string& expected)
    {
        ASSERT_FALSE(future.isCanceled());
        ASSERT_TRUE(future.isValid());
        ASSERT_EQ(future.resultCount(), 1);
        ASSERT_EQ(payload(future.result()), expected);
    }

    void thenLoadFailed() { ASSERT_FALSE(future.isValid()); }

    void thenSourceRequested(const QnTimePeriod& period, int limit)
    {
        ASSERT_EQ(source->log().size(), 1);
        ASSERT_EQ(source->log().pop(), (Request{period, limit}));
    }

    void thenServedFromCache() { ASSERT_TRUE(source->log().empty()); }

protected:
    QnSyncTime timeSource;
    std::shared_ptr<AbstractTestBackend> source;
    std::shared_ptr<CachingProxyBackend<TestDataList>> backend;
    QFuture<TestDataList> future;
};

using SourceBackendTypes = ::testing::Types<AsyncTestBackend, SyncTestBackend>;
TYPED_TEST_SUITE(TimelineCachingProxyBackendTest, SourceBackendTypes);

TYPED_TEST(TimelineCachingProxyBackendTest, notCached)
{
    this->whenLoaded(interval(9ms, 0ms), /*limit*/ 8);
    this->thenSourceRequested(interval(9ms, 0ms), /*limit*/ 8);
    this->thenLoadResultIs("ABCDEFGH");
}

TYPED_TEST(TimelineCachingProxyBackendTest, fullyCached)
{
    this->givenCached(interval(9ms, 0ms), /*limit*/ 8);

    this->whenLoaded(interval(9ms, 0ms), /*limit*/ 8);
    this->thenServedFromCache();
    this->thenLoadResultIs("ABCDEFGH");

    this->whenLoaded(interval(9ms, 5ms), kUnlimited);
    this->thenServedFromCache();
    this->thenLoadResultIs("ABCDE");

    this->whenLoaded(interval(3ms, 0ms), /*limit*/ 1);
    this->thenServedFromCache();
    this->thenLoadResultIs("H");
}

TYPED_TEST(TimelineCachingProxyBackendTest, partiallyCachedByPeriod)
{
    this->givenCached(interval(9ms, 0ms), /*limit*/ 8);
    this->whenLoaded(interval(10ms, 0ms), /*limit*/ 8);
    this->thenSourceRequested(interval(10ms, 0ms), /*limit*/ 8);
    this->thenLoadResultIs("ABCDEFGH");
}

TYPED_TEST(TimelineCachingProxyBackendTest, partiallyCachedByLimit)
{
    this->givenCached(interval(9ms, 0ms), /*limit*/ 8);
    this->whenLoaded(interval(9ms, 0ms), /*limit*/ 10);
    this->thenSourceRequested(interval(9ms, 0ms), /*limit*/ 10);
    this->thenLoadResultIs("ABCDEFGHIJ");
}

TYPED_TEST(TimelineCachingProxyBackendTest, expirableIsNotExpiredRightAway)
{
    this->givenExpirationTime(1s);
    this->givenCached(interval(9ms, 0ms), /*limit*/ 8);
    this->whenLoaded(interval(9ms, 0ms), /*limit*/ 8);
    this->thenServedFromCache();
    this->thenLoadResultIs("ABCDEFGH");
}

TYPED_TEST(TimelineCachingProxyBackendTest, alreadyExpiredIsExpired)
{
    this->givenExpirationTime(0s);
    this->givenCached(interval(9ms, 0ms), /*limit*/ 8);
    this->whenLoaded(interval(9ms, 0ms), /*limit*/ 8);
    this->thenSourceRequested(interval(9ms, 0ms), /*limit*/ 8);
    this->thenLoadResultIs("ABCDEFGH");
}

TYPED_TEST(TimelineCachingProxyBackendTest, expirabilityByAge)
{
    constexpr auto kAgeThreshold = 1h;

    this->givenExpirationTime(0s); //< All expirable blocks will expire immediately.
    this->givenAgeThreshold(kAgeThreshold);

    const QnTimePeriod overThresholdPeriod(this->agedTimePoint(kAgeThreshold) - 1s, 1s);
    this->whenLoaded(overThresholdPeriod, kUnlimited);
    this->thenSourceRequested(overThresholdPeriod, kUnlimited);
    this->thenLoadResultIs("");

    // A cached block that's over the age threshold is not expirable.
    this->whenLoaded(overThresholdPeriod, kUnlimited);
    this->thenServedFromCache();
    this->thenLoadResultIs("");

    const QnTimePeriod underThresholdPeriod(this->agedTimePoint(kAgeThreshold) + 1min, 1s);
    this->whenLoaded(underThresholdPeriod, kUnlimited);
    this->thenSourceRequested(underThresholdPeriod, kUnlimited);
    this->thenLoadResultIs("");

    // A cached block that's under the age threshold is expirable.
    this->whenLoaded(underThresholdPeriod, kUnlimited);
    this->thenSourceRequested(underThresholdPeriod, kUnlimited);
    this->thenLoadResultIs("");
}

TYPED_TEST(TimelineCachingProxyBackendTest, invalidateCachedPeriod)
{
    this->givenCached(interval(9ms, 0ms), kUnlimited);
    this->whenInvalidated(interval(9ms, 0ms));
    this->whenLoaded(interval(9ms, 0ms), kUnlimited);
    this->thenSourceRequested(interval(9ms, 0ms), kUnlimited);
    this->thenLoadResultIs("ABCDEFGHIJKLM");
}

TYPED_TEST(TimelineCachingProxyBackendTest, invalidateEncompassingPeriod)
{
    this->givenCached(interval(6ms, 3ms), kUnlimited);
    this->whenInvalidated(interval(9ms, 0ms));
    this->whenLoaded(interval(6ms, 3ms), kUnlimited);
    this->thenSourceRequested(interval(6ms, 3ms), kUnlimited);
    this->thenLoadResultIs("ABCDEFGHIJ");
}

TYPED_TEST(TimelineCachingProxyBackendTest, invalidateNotIntersectingPeriod)
{
    this->givenCached(interval(9ms, 5ms), kUnlimited);
    this->whenInvalidated(interval(4ms, 0ms));
    this->whenLoaded(interval(9ms, 5ms), kUnlimited);
    this->thenServedFromCache();
    this->thenLoadResultIs("ABCDE");
}

TYPED_TEST(TimelineCachingProxyBackendTest, invalidatePartOfCachedPeriod)
{
    this->givenCached(interval(9ms, 0ms), kUnlimited);
    this->whenInvalidated(interval(2ms, 0ms));

    // The rest of the cached period is still served from the cache.
    this->whenLoaded(interval(9ms, 3ms), kUnlimited);
    this->thenServedFromCache();
    this->thenLoadResultIs("ABCDEFGHIJ");

    // A period intersecting the invalidated one is requested from the source backend.
    this->whenLoaded(interval(9ms, 0ms), kUnlimited);
    this->thenSourceRequested(interval(9ms, 0ms), kUnlimited);
    this->thenLoadResultIs("ABCDEFGHIJKLM");
}

TYPED_TEST(TimelineCachingProxyBackendTest, invalidateMiddleOfCachedPeriod)
{
    this->givenCached(interval(9ms, 0ms), kUnlimited);
    this->whenInvalidated(interval(5ms, 4ms));

    // Both parts around the invalidated period are still served from the cache.
    this->whenLoaded(interval(9ms, 6ms), kUnlimited);
    this->thenServedFromCache();
    this->thenLoadResultIs("ABC");

    this->whenLoaded(interval(3ms, 0ms), kUnlimited);
    this->thenServedFromCache();
    this->thenLoadResultIs("HIJKLM");

    // A period intersecting the invalidated one is requested from the source backend.
    this->whenLoaded(interval(9ms, 0ms), kUnlimited);
    this->thenSourceRequested(interval(9ms, 0ms), kUnlimited);
    this->thenLoadResultIs("ABCDEFGHIJKLM");
}

TYPED_TEST(TimelineCachingProxyBackendTest, sourceFailed)
{
    this->givenSourceFailing();
    this->whenLoaded(interval(9ms, 0ms), kUnlimited);
    this->thenSourceRequested(interval(9ms, 0ms), kUnlimited);
    this->thenLoadFailed();

    // A retry requesting the source proves that nothing was added to the cache.
    this->whenSourceRecovered();
    this->whenLoaded(interval(9ms, 0ms), kUnlimited);
    this->thenSourceRequested(interval(9ms, 0ms), kUnlimited);
    this->thenLoadResultIs("ABCDEFGHIJKLM");
}

// ------------------------------------------------------------------------------------------------
// Invalidation of running requests.

namespace {

/** Test data item invoking a one-shot hook when copied. */
struct CopyHookedData
{
    CopyHookedData(milliseconds timestamp, char payload): timestamp(timestamp), payload(payload) {}

    CopyHookedData(const CopyHookedData& other):
        timestamp(other.timestamp),
        payload(other.payload)
    {
        if (copyHook)
            std::exchange(copyHook, {})();
    }

    CopyHookedData(CopyHookedData&&) = default;
    CopyHookedData& operator=(const CopyHookedData&) = default;
    CopyHookedData& operator=(CopyHookedData&&) = default;

    milliseconds timestamp;
    char payload;

    static inline std::function<void()> copyHook;
};

using CopyHookedDataList = std::vector<CopyHookedData>;

} // namespace

} // namespace test

template<>
struct DataListTraits<test::CopyHookedDataList>
{
    static std::chrono::milliseconds timestamp(const auto& item) { return item.timestamp; }
};

namespace test {

namespace {

std::string payload(const CopyHookedDataList& list)
{
    std::string result;
    for (const auto& item: list)
        result.push_back(item.payload);
    return result;
}

/** Keeps load requests running until the test makes it respond. */
template<typename DataList>
class DeferredBackendMock:
    public AbstractBackend<DataList>,
    public DataListHelpers<DataList>
{
public:
    explicit DeferredBackendMock(DataList data): m_data(std::move(data)) {}

    virtual QnResourcePtr resource() const override { return {}; }

    virtual QFuture<DataList> load(const QnTimePeriod& period, int limit) override
    {
        if (m_beforeLoadHook)
            std::exchange(m_beforeLoadHook, {})();

        m_log.push(period, limit);
        auto& request = m_requests.emplace_back(period, limit);
        request.promise.start();
        return request.promise.future();
    }

    /** `beforeFinish` is invoked when the result is set, but not reported yet. */
    void respond(int index, const std::function<void()>& beforeFinish = {})
    {
        auto& request = m_requests.at(index);
        request.promise.emplaceResult(this->slice(m_data, request.period, request.limit));
        if (beforeFinish)
            beforeFinish();
        request.promise.finish();
    }

    bool isCanceled(int index) const { return m_requests.at(index).promise.isCanceled(); }

    RequestLog& log() { return m_log; }

    /** One-shot hook invoked at the beginning of the next `load`. */
    void setBeforeLoadHook(std::function<void()> value) { m_beforeLoadHook = std::move(value); }

private:
    struct PendingRequest
    {
        PendingRequest(const QnTimePeriod& period, int limit): period(period), limit(limit) {}

        const QnTimePeriod period;
        const int limit;
        QPromise<DataList> promise;
    };

    const DataList m_data;
    std::deque<PendingRequest> m_requests;
    RequestLog m_log;
    std::function<void()> m_beforeLoadHook;
};

} // namespace

class TimelineCachingProxyBackendInvalidationTest: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        source = std::make_shared<DeferredBackendMock<CopyHookedDataList>>(CopyHookedDataList{
            {6ms, 'A'},
            {5ms, 'B'},
            {4ms, 'C'},
            {3ms, 'D'},
            {2ms, 'E'},
            {1ms, 'F'}});

        backend = std::make_shared<CachingProxyBackend<CopyHookedDataList>>(source, &timeSource);
        backend->setAgeThreshold(std::nullopt);
    }

    virtual void TearDown() override
    {
        CopyHookedData::copyHook = {};
        futures.clear();
        backend.reset();
        source.reset();
    }

    void givenRunningLoad(const QnTimePeriod& period) { whenLoadStarted(period); }

    void givenInvalidationWhileSending(const QnTimePeriod& period)
    {
        source->setBeforeLoadHook([this, period]() { backend->invalidate(period); });
    }

    void whenLoadStarted(const QnTimePeriod& period)
    {
        futures.push_back(backend->load(period, kUnlimited));
    }

    void whenInvalidated(const QnTimePeriod& period) { backend->invalidate(period); }

    void whenSourceResponds() { source->respond(/*index*/ 0); }

    /**
     * Invalidates the period when the response is already being delivered to the caching proxy,
     * but isn't added to its cache yet. That's when the proxy's continuation copies the result,
     * see Qt's `CompactContinuation::fulfillPromiseWithResult`.
     */
    void whenSourceRespondsInvalidatingOnDelivery(const QnTimePeriod& period)
    {
        source->respond(/*index*/ 0,
            [this, period]()
            {
                CopyHookedData::copyHook =
                    [this, period]()
                    {
                        backend->invalidate(period);
                        invalidatedOnDelivery = true;
                    };
            });
    }

    void whenLoadRepeated(const QnTimePeriod& period)
    {
        source->log().clear();
        whenLoadStarted(period);
    }

    void thenSourceRequestCanceled() { ASSERT_TRUE(source->isCanceled(0)); }

    void thenLoadCanceled() { ASSERT_TRUE(futures.front().isCanceled()); }

    void thenLoadResultIs(const std::string& expected)
    {
        const auto& future = futures.front();
        ASSERT_TRUE(future.isFinished());
        ASSERT_FALSE(future.isCanceled());
        ASSERT_EQ(payload(future.result()), expected);
    }

    void thenInvalidatedOnDelivery() { ASSERT_TRUE(invalidatedOnDelivery); }

    void thenSourceRequested() { ASSERT_EQ(source->log().size(), 1); }

    void thenServedFromCache() { ASSERT_TRUE(source->log().empty()); }

protected:
    QnSyncTime timeSource;
    std::shared_ptr<DeferredBackendMock<CopyHookedDataList>> source;
    std::shared_ptr<CachingProxyBackend<CopyHookedDataList>> backend;
    std::vector<QFuture<CopyHookedDataList>> futures;
    bool invalidatedOnDelivery = false;
};

TEST_F(TimelineCachingProxyBackendInvalidationTest, intersectingRunningLoadIsCanceled)
{
    givenRunningLoad(interval(6ms, 1ms));
    whenInvalidated(interval(4ms, 4ms));
    thenSourceRequestCanceled();

    whenSourceResponds();
    thenLoadCanceled();

    whenLoadRepeated(interval(6ms, 1ms));
    thenSourceRequested();
}

TEST_F(TimelineCachingProxyBackendInvalidationTest, notIntersectingRunningLoadIsCached)
{
    givenRunningLoad(interval(3ms, 1ms));
    whenInvalidated(interval(6ms, 6ms));
    whenSourceResponds();
    thenLoadResultIs("DEF");

    whenLoadRepeated(interval(3ms, 1ms));
    thenServedFromCache();
}

TEST_F(TimelineCachingProxyBackendInvalidationTest, invalidationWhileSendingCancels)
{
    givenInvalidationWhileSending(interval(4ms, 4ms));
    whenLoadStarted(interval(6ms, 1ms));
    thenSourceRequestCanceled();

    whenSourceResponds();
    thenLoadCanceled();
}

// This test depends on a Qt implementation detail: a copyable result is copied into a `then`
// continuation outside of any lock (`CompactContinuation::fulfillPromiseWithResult` in Qt 6.11).
// After a Qt upgrade it may break: if the result is no longer copied, `thenInvalidatedOnDelivery`
// fails; if it's copied under a lock, the test deadlocks. In that case find another way to run
// code in the continuation before the proxy adds the result to the cache. Don't just remove the
// test: the race is real.
TEST_F(TimelineCachingProxyBackendInvalidationTest, invalidationOnDeliveryIsNotCached)
{
    givenRunningLoad(interval(6ms, 1ms));
    whenSourceRespondsInvalidatingOnDelivery(interval(4ms, 4ms));
    thenInvalidatedOnDelivery();

    whenLoadRepeated(interval(6ms, 1ms));
    thenSourceRequested();
}

TEST_F(TimelineCachingProxyBackendInvalidationTest, invalidationAfterCompletionInvalidatesCache)
{
    givenRunningLoad(interval(6ms, 1ms));
    whenSourceResponds();
    whenInvalidated(interval(4ms, 4ms));
    thenLoadResultIs("ABCDEF");

    whenLoadRepeated(interval(6ms, 1ms));
    thenSourceRequested();
}

} // namespace test
} // namespace nx::vms::client::core::timeline
