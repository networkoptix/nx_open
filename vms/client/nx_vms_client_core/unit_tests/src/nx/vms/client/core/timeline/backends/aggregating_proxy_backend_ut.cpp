// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>

#include <nx/vms/client/core/timeline/backends/aggregating_proxy_backend.h>

#include "test_backends.h"

namespace nx::vms::client::core::timeline {
namespace test {

using namespace std::chrono;

template<typename SourceBackend>
class TimelineAggregatingProxyBackendTest: public ::testing::Test
{
    using base_type = ::testing::Test;

public:
    std::shared_ptr<AbstractTestBackend> source;
    std::shared_ptr<AggregatingProxyBackend<TestDataList>> backend;

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

        backend = std::make_shared<AggregatingProxyBackend<TestDataList>>(source);
        backend->setAggregationInterval(1ms); //< Minimal delay for tests; doesn't change the logic.
    }

    virtual void TearDown() override
    {
        backend.reset();
        source.reset();
        base_type::TearDown();
    }
};

#define LOAD this->backend->load
#define LOG this->source->log

#define WAIT_AND_CHECK(future, expected)\
    ASSERT_TRUE(!future.isCanceled());\
    waitForCompletion(future);\
    ASSERT_TRUE(future.isValid());\
    ASSERT_EQ(future.resultCount(), 1);\
    ASSERT_EQ(payload(future.result()), expected)

using SourceBackendTypes = ::testing::Types<AsyncTestBackend, SyncTestBackend>;
TYPED_TEST_SUITE(TimelineAggregatingProxyBackendTest, SourceBackendTypes);

TYPED_TEST(TimelineAggregatingProxyBackendTest, singleRequest)
{
    const auto future = LOAD(interval(9ms, 0ms), /*limit*/ 6);
    WAIT_AND_CHECK(future, "ABCDEF");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(9ms, 0ms), /*limit*/ 6}));
}

TYPED_TEST(TimelineAggregatingProxyBackendTest, nonIntersectingRequests)
{
    const auto future1 = LOAD(interval(2ms, 0ms), /*limit*/ 3);
    const auto future2 = LOAD(interval(9ms, 4ms), /*limit*/ 3);
    WAIT_AND_CHECK(future1, "KLM");
    WAIT_AND_CHECK(future2, "ABC");
    ASSERT_EQ(LOG().size(), 2);
    ASSERT_EQ(LOG().pop(), (Request{interval(2ms, 0ms), /*limit*/ 3}));
    ASSERT_EQ(LOG().pop(), (Request{interval(9ms, 4ms), /*limit*/ 3}));
}

TYPED_TEST(TimelineAggregatingProxyBackendTest, touchingRequests)
{
    // In this test, two requests with limit 4 are combined and sent with limit 8.
    // Limit 8 is enough to cover the entire combined period, so no additional requests are sent.

    const auto future1 = LOAD(interval(3ms, 2ms), /*limit*/ 4);
    const auto future2 = LOAD(interval(5ms, 4ms), /*limit*/ 4);
    WAIT_AND_CHECK(future1, "HIJK");
    WAIT_AND_CHECK(future2, "DEFG");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(5ms, 2ms), /*limit*/ 8}));
}

TYPED_TEST(TimelineAggregatingProxyBackendTest, intersectingRequests)
{
    // In this test, two requests with limit 4 are combined and sent with limit 8.
    // Limit 8 is enough to cover the entire combined period, so no additional requests are sent.

    const auto future1 = LOAD(interval(3ms, 2ms), /*limit*/ 4);
    const auto future2 = LOAD(interval(5ms, 3ms), /*limit*/ 4);
    WAIT_AND_CHECK(future1, "HIJK");
    WAIT_AND_CHECK(future2, "DEFG");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(5ms, 2ms), /*limit*/ 8}));
}

TYPED_TEST(TimelineAggregatingProxyBackendTest, samePeriodRequests)
{
    // Requests with the same period but different limits are handled exactly as intersecting.

    const auto future1 = LOAD(interval(5ms, 4ms), /*limit*/ 4);
    const auto future2 = LOAD(interval(5ms, 4ms), /*limit*/ 2);
    WAIT_AND_CHECK(future1, "DEFG");
    WAIT_AND_CHECK(future2, "DE");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(5ms, 4ms), /*limit*/ 6}));
}

TYPED_TEST(TimelineAggregatingProxyBackendTest, duplicateRequests)
{
    // Requests with the same period and limit are handled exactly as intersecting.

    const auto future1 = LOAD(interval(5ms, 4ms), /*limit*/ 4);
    const auto future2 = LOAD(interval(5ms, 4ms), /*limit*/ 4);
    WAIT_AND_CHECK(future1, "DEFG");
    WAIT_AND_CHECK(future2, "DEFG");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(5ms, 4ms), /*limit*/ 8}));
}

TYPED_TEST(TimelineAggregatingProxyBackendTest, twoDomainsOfRequests)
{
    // 4 requests
    // #1 and #2 are touching and form the 1st domain
    // #3 and #4 are intersecting and form the 2nd domain

    const auto future1 = LOAD(interval(5ms, 4ms), /*limit*/ 4);
    const auto future2 = LOAD(interval(9ms, 6ms), /*limit*/ 4);
    const auto future3 = LOAD(interval(2ms, 2ms), /*limit*/ 4);
    const auto future4 = LOAD(interval(1ms, 0ms), /*limit*/ 4);
    WAIT_AND_CHECK(future1, "DEFG");
    WAIT_AND_CHECK(future2, "ABC");
    WAIT_AND_CHECK(future3, "K");
    WAIT_AND_CHECK(future4, "LM");
    ASSERT_EQ(LOG().size(), 2);
    ASSERT_EQ(LOG().pop(), (Request{interval(2ms, 0ms), /*limit*/ 8}));
    ASSERT_EQ(LOG().pop(), (Request{interval(9ms, 4ms), /*limit*/ 8}));
}

TYPED_TEST(TimelineAggregatingProxyBackendTest, emptyResponses)
{
    const auto future1 = LOAD(interval(0ms, 0ms), /*limit*/ 3);
    const auto future2 = LOAD(interval(8ms, 8ms), /*limit*/ 3);
    const auto future3 = LOAD(interval(9ms, 7ms), /*limit*/ 3);
    WAIT_AND_CHECK(future1, "");
    WAIT_AND_CHECK(future2, "");
    WAIT_AND_CHECK(future3, "");
    ASSERT_EQ(LOG().size(), 2);
    ASSERT_EQ(LOG().pop(), (Request{interval(0ms, 0ms), /*limit*/ 3}));
    ASSERT_EQ(LOG().pop(), (Request{interval(9ms, 7ms), /*limit*/ 6}));
}

TYPED_TEST(TimelineAggregatingProxyBackendTest, cancelationInQueue)
{
    auto future1 = LOAD(interval(5ms, 1ms), /*limit*/ 4);
    const auto future2 = LOAD(interval(5ms, 4ms), /*limit*/ 4);
    future1.cancel();
    WAIT_AND_CHECK(future2, "DEFG");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(5ms, 4ms), /*limit*/ 4}));
}

TYPED_TEST(TimelineAggregatingProxyBackendTest, cancelationAfterSend)
{
    auto future1 = LOAD(interval(5ms, 1ms), /*limit*/ 4);
    const auto future2 = LOAD(interval(5ms, 4ms), /*limit*/ 4);
    this->source->setBeforeLoadHook([&]() { future1.cancel(); });
    WAIT_AND_CHECK(future2, "DEFG");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(5ms, 1ms), /*limit*/ 8}));
}

TYPED_TEST(TimelineAggregatingProxyBackendTest, followUpNotAggregated)
{
    // If only one queued request is fulfilled by the aggregated request response,
    // the rest is re-requested without aggregation.
    // The aggregated request is [2ms, 6ms], limit=6, and returns "ABCDEF", fulfilling only
    // request #1. As a follow-up, request #2 and request #3 are sent without aggregation.

    const auto future1 = LOAD(interval(6ms, 5ms), /*limit*/ 2);
    const auto future2 = LOAD(interval(3ms, 2ms), /*limit*/ 2);
    const auto future3 = LOAD(interval(4ms, 3ms), /*limit*/ 2);
    WAIT_AND_CHECK(future1, "AB");
    WAIT_AND_CHECK(future2, "HI");
    WAIT_AND_CHECK(future3, "FG");
    ASSERT_EQ(LOG().size(), 3);
    ASSERT_EQ(LOG().pop(), (Request{interval(6ms, 2ms), /*limit*/ 6}));
    ASSERT_EQ(LOG().pop(), (Request{interval(3ms, 2ms), /*limit*/ 2}));
    ASSERT_EQ(LOG().pop(), (Request{interval(4ms, 3ms), /*limit*/ 2}));
}

TYPED_TEST(TimelineAggregatingProxyBackendTest, followUpAggregated)
{
    // If more then one queued request is fulfilled by the aggregated request response,
    // the rest is sent with aggregation, possibly with newly queued requests.
    // The aggregated request is [2ms, 6ms], limit=8, and returns "ABCDEFGH", fulfilling #1 and #2.
    // As a follow-up, #3 and #4 are re-aggregated together with #5 which was queued during
    // the initial aggregated request asynchronous processing.

    auto future1 = LOAD(interval(6ms, 5ms), /*limit*/ 2); //< #1.
    const auto future2 = LOAD(interval(5ms, 4ms), /*limit*/ 2); //< #2.
    const auto future3 = LOAD(interval(3ms, 3ms), /*limit*/ 2); //< #3.
    const auto future4 = LOAD(interval(3ms, 2ms), /*limit*/ 2); //< #4.

    QFuture<TestDataList> future5;
    const auto future1_with_continuation = future1.then(
        [&](TestDataList result)
        {
            // This will queue up when #1 is fulfilled, eventually getting aggregated into a single
            // block with not fulfilled #3 and #4.
            future5 = LOAD(interval(2ms, 1ms), /*limit*/ 2);
            return result;
        });

    WAIT_AND_CHECK(future1_with_continuation, "AB");
    WAIT_AND_CHECK(future2, "DE");
    WAIT_AND_CHECK(future3, "HI");
    WAIT_AND_CHECK(future4, "HI");
    WAIT_AND_CHECK(future5, "KL");

    ASSERT_EQ(LOG().size(), 2);
    ASSERT_EQ(LOG().pop(), (Request{interval(6ms, 2ms), /*limit*/ 8}));
    ASSERT_EQ(LOG().pop(), (Request{interval(3ms, 1ms), /*limit*/ 6}));
}

TYPED_TEST(TimelineAggregatingProxyBackendTest, sourceFailed)
{
    this->source->setFailureMode(true);
    auto future = LOAD(interval(9ms, 0ms), kUnlimited);
    waitForCompletion(future);
    ASSERT_FALSE(future.isValid());
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(9ms, 0ms), kUnlimited}));

    this->source->setFailureMode(false);
    future = LOAD(interval(9ms, 0ms), kUnlimited);
    WAIT_AND_CHECK(future, "ABCDEFGHIJKLM");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(9ms, 0ms), kUnlimited}));
}

} // namespace test
} // namespace nx::vms::client::core::timeline
