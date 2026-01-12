// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <chrono>
#include <memory>

#include <nx/vms/client/core/timeline/backends/caching_proxy_backend.h>
#include <utils/common/synctime.h>

#include "test_backends.h"

// Most of the caching behavior tests are implemented in `data_lists_cache_ut.cpp`.
// The purpose of this suite is to ensure the interaction with the source backend is correct.

namespace nx::vms::client::core::timeline {
namespace test {

using namespace std::chrono;

template<typename SourceBackend>
class TimelineCachingProxyBackendTest: public ::testing::Test
{
    using base_type = ::testing::Test;

public:
    QnSyncTime timeSource;
    std::shared_ptr<AbstractTestBackend> source;
    std::shared_ptr<CachingProxyBackend<TestDataList>> backend;

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
TYPED_TEST_SUITE(TimelineCachingProxyBackendTest, SourceBackendTypes);

TYPED_TEST(TimelineCachingProxyBackendTest, notCached)
{
    const auto future = LOAD(interval(9ms, 0ms), /*limit*/ 8);
    WAIT_AND_CHECK(future, "ABCDEFGH");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(9ms, 0ms), /*limit*/ 8}));
}

TYPED_TEST(TimelineCachingProxyBackendTest, fullyCached)
{
    waitForCompletion(LOAD(interval(9ms, 0ms), /*limit*/ 8));
    LOG().clear();

    auto future = LOAD(interval(9ms, 0ms), /*limit*/ 8);
    WAIT_AND_CHECK(future, "ABCDEFGH");
    ASSERT_TRUE(LOG().empty());

    future = LOAD(interval(9ms, 5ms), kUnlimited);
    WAIT_AND_CHECK(future, "ABCDE");
    ASSERT_TRUE(LOG().empty());

    future = LOAD(interval(3ms, 0ms), /*limit*/ 1);
    WAIT_AND_CHECK(future, "H");
    ASSERT_TRUE(LOG().empty());
}

TYPED_TEST(TimelineCachingProxyBackendTest, partiallyCachedByPeriod)
{
    waitForCompletion(LOAD(interval(9ms, 0ms), /*limit*/ 8));
    LOG().clear();

    const auto future = LOAD(interval(10ms, 0ms), /*limit*/ 8);
    WAIT_AND_CHECK(future, "ABCDEFGH");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(10ms, 0ms), /*limit*/ 8}));
}

TYPED_TEST(TimelineCachingProxyBackendTest, partiallyCachedByLimit)
{
    waitForCompletion(LOAD(interval(9ms, 0ms), /*limit*/ 8));
    LOG().clear();

    const auto future = LOAD(interval(9ms, 0ms), /*limit*/ 10);
    WAIT_AND_CHECK(future, "ABCDEFGHIJ");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(9ms, 0ms), /*limit*/ 10}));
}

TYPED_TEST(TimelineCachingProxyBackendTest, expirableIsNotExpiredRightAway)
{
    this->backend->setExpirationTime(1s);
    waitForCompletion(LOAD(interval(9ms, 0ms), /*limit*/ 8));
    LOG().clear();

    auto future = LOAD(interval(9ms, 0ms), /*limit*/ 8);
    WAIT_AND_CHECK(future, "ABCDEFGH");
    ASSERT_TRUE(LOG().empty());
}

TYPED_TEST(TimelineCachingProxyBackendTest, alreadyExpiredIsExpired)
{
    this->backend->setExpirationTime(0s);
    waitForCompletion(LOAD(interval(9ms, 0ms), /*limit*/ 8));
    LOG().clear();

    auto future = LOAD(interval(9ms, 0ms), /*limit*/ 8);
    WAIT_AND_CHECK(future, "ABCDEFGH");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(9ms, 0ms), /*limit*/ 8}));
}

TYPED_TEST(TimelineCachingProxyBackendTest, expirabilityByAge)
{
    constexpr auto kAgeThreshold = 1h;

    this->backend->setExpirationTime(0s); //< All expirable blocks will expire immediately.
    this->backend->setAgeThreshold(kAgeThreshold);

    const auto thresholdPoint =
        [this, kAgeThreshold]() { return this->timeSource.value() - kAgeThreshold; };

    const QnTimePeriod overThresholdPeriod(thresholdPoint() - 1s, 1s);
    auto future = LOAD(overThresholdPeriod, kUnlimited);
    WAIT_AND_CHECK(future, "");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{overThresholdPeriod, kUnlimited}));

    // A cached block that's over the age threshold is not expirable.
    future = LOAD(overThresholdPeriod, kUnlimited);
    WAIT_AND_CHECK(future, "");
    ASSERT_TRUE(LOG().empty());

    const QnTimePeriod underThresholdPeriod(thresholdPoint() + 1min, 1s);
    future = LOAD(underThresholdPeriod, kUnlimited);
    WAIT_AND_CHECK(future, "");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{underThresholdPeriod, kUnlimited}));

    // A cached block that's under the age threshold is expirable.
    future = LOAD(underThresholdPeriod, kUnlimited);
    WAIT_AND_CHECK(future, "");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{underThresholdPeriod, kUnlimited}));
}

TYPED_TEST(TimelineCachingProxyBackendTest, sourceFailed)
{
    this->source->setFailureMode(true);
    auto future = LOAD(interval(9ms, 0ms), kUnlimited);
    waitForCompletion(future);
    ASSERT_FALSE(future.isValid());
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(9ms, 0ms), kUnlimited}));

    // A retry re-requesting from the source proves there was nothing added to the cache:
    this->source->setFailureMode(false);
    future = LOAD(interval(9ms, 0ms), kUnlimited);
    WAIT_AND_CHECK(future, "ABCDEFGHIJKLM");
    ASSERT_EQ(LOG().size(), 1);
    ASSERT_EQ(LOG().pop(), (Request{interval(9ms, 0ms), kUnlimited}));
}

} // namespace test
} // namespace nx::vms::client::core::timeline
