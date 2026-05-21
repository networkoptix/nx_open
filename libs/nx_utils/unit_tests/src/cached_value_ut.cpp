// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <future>
#include <thread>
#include <vector>
#include <nx/utils/value_cache.h>
#include <nx/utils/scope_guard.h>

namespace nx::utils::test {

using namespace std::chrono_literals;

class SpecialFunctionsCatcher
{
public:
    virtual ~SpecialFunctionsCatcher() = default;

    virtual void construct() = 0;
    virtual void destruct() = 0;
    virtual void copyConstruct() = 0;
    virtual void copyAssign() = 0;
};

class SpecialFunctionsNullCatcher: public SpecialFunctionsCatcher
{
public:
    virtual void construct() override {}
    virtual void destruct() override {}
    virtual void copyConstruct() override {}
    virtual void copyAssign() override {}
};

class SomeType
{
public:
    static SpecialFunctionsNullCatcher m_defaultCatcher;
    static SpecialFunctionsCatcher* m_catcher;

public:

    SomeType(): m_value(0) {}

    SomeType(int value): m_value(value)  { m_catcher->construct(); }
    ~SomeType() { m_catcher->destruct(); }

    SomeType(const SomeType& other): m_value(other.m_value) { m_catcher->copyConstruct(); }
    SomeType& operator=(const SomeType& other)
    {
        m_value = other.m_value;
        m_catcher->copyAssign();
        return *this;
    }

    SomeType(SomeType&&) = default;
    SomeType& operator=(SomeType&&) = default;

    int m_value;
};

SpecialFunctionsNullCatcher SomeType::m_defaultCatcher;
SpecialFunctionsCatcher* SomeType::m_catcher = &SomeType::m_defaultCatcher;

class SpecialFunctionsCatcherMock: public SpecialFunctionsCatcher
{
public:
    MOCK_METHOD(void, construct, ());
    MOCK_METHOD(void, destruct, ());
    MOCK_METHOD(void, copyConstruct, ());
    MOCK_METHOD(void, copyAssign, ());
};

static void expectSpecialFunctionCalls(
    SpecialFunctionsCatcherMock& catcherMock,
    int constructCount,
    int destructCount,
    int copyConstructCount,
    int copyAssignCount)
{
    SomeType::m_catcher = &catcherMock;

    if (constructCount >= 0)
        EXPECT_CALL(catcherMock, construct()).Times(constructCount);

    if (destructCount >= 0)
        EXPECT_CALL(catcherMock, destruct()).Times(destructCount);

    if (copyConstructCount >= 0)
        EXPECT_CALL(catcherMock, copyConstruct()).Times(copyConstructCount);

    if (copyAssignCount >= 0)
        EXPECT_CALL(catcherMock, copyAssign()).Times(copyAssignCount);
}

TEST(CachedValue, Base)
{
     int returnValue = 0;
     CachedValue<int> value([&returnValue](){ return returnValue; });

     ASSERT_EQ(value.get(), 0);

     returnValue += 1;
     ASSERT_EQ(value.get(), 0);
     value.reset();
     ASSERT_EQ(value.get(), 1);

     returnValue += 1;
     ASSERT_EQ(value.get(), 1);
     ASSERT_EQ(value.updated(), 2);
}

TEST(CachedValue, ConstructionAndCopy)
{
    ::testing::NiceMock<SpecialFunctionsCatcherMock> catcherMocks[6];
    auto guard = nx::utils::makeScopeGuard(
        [](){ SomeType::m_catcher = &SomeType::m_defaultCatcher; });

    expectSpecialFunctionCalls(catcherMocks[0], 0, -1, -1, -1);
    CachedValue<SomeType> value([n = 0]() mutable { return SomeType(n++); });

    expectSpecialFunctionCalls(catcherMocks[1], 1, -1, 1, 0);
    EXPECT_EQ(value.get().m_value, 0);

    expectSpecialFunctionCalls(catcherMocks[2], 0, -1, 1, 0);
    EXPECT_EQ(value.get().m_value, 0);

    expectSpecialFunctionCalls(catcherMocks[3], 1, 1, 0, 0);
    value.update();

    expectSpecialFunctionCalls(catcherMocks[4], 1, 1, 0, 0);
    value.update();

    expectSpecialFunctionCalls(catcherMocks[5], 0, 1, 0, 0);
    value.reset();
}

TEST(CachedValue, ExpirationTime)
{
    ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    constexpr std::chrono::milliseconds expiryTime(8);
    ::testing::NiceMock<SpecialFunctionsCatcherMock> catcherMocks[8];
    auto guard = nx::utils::makeScopeGuard(
        [](){ SomeType::m_catcher = &SomeType::m_defaultCatcher; });

    expectSpecialFunctionCalls(catcherMocks[0], 1, -1, 1, 0);
    CachedValueWithTimeout<SomeType> value([n = 0]() mutable { return SomeType(n++); }, expiryTime);
    EXPECT_EQ(value.get().m_value, 0);

    // Nothing should happen before any call to CachedValue.
    expectSpecialFunctionCalls(catcherMocks[1], 0, 0, 0, 0);
    timeShift.applyRelativeShift(expiryTime + 1ms);

    // New value should be generated after timeout.
    expectSpecialFunctionCalls(catcherMocks[2], 1, -1, 1, 0);
    EXPECT_EQ(value.get().m_value, 1);

    // No new value generated before timeout.
    expectSpecialFunctionCalls(catcherMocks[3], 0, -1, 1, 0);
    timeShift.applyRelativeShift(expiryTime / 2);
    EXPECT_EQ(value.get().m_value, 1);

    // reset() invalidates without doing the work; next get() regenerates and restarts timer.
    expectSpecialFunctionCalls(catcherMocks[4], 1, -1, 1, 0);
    value.reset();
    EXPECT_EQ(value.get().m_value, 2);

    // Make sure timer was restarted by the get() above (half-expiry should not trigger refresh).
    expectSpecialFunctionCalls(catcherMocks[5], 0, -1, 1, 0);
    timeShift.applyRelativeShift(expiryTime / 2 + 1ms);
    EXPECT_EQ(value.get().m_value, 2);

    // Make sure value regenerated after full timeout.
    expectSpecialFunctionCalls(catcherMocks[6], 1, -1, 1, 0);
    timeShift.applyRelativeShift(expiryTime / 2 + 1ms);
    EXPECT_EQ(value.get().m_value, 3);

    expectSpecialFunctionCalls(catcherMocks[7], 0, 1, 0, 0);
    value.reset();
}

TEST(CachedValue, LongTimeOfValueGeneration)
{
    ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    constexpr std::chrono::milliseconds expiryTime(2);
    ::testing::NiceMock<SpecialFunctionsCatcherMock> catcherMocks[8];
    auto guard = nx::utils::makeScopeGuard(
        [](){ SomeType::m_catcher = &SomeType::m_defaultCatcher; });

    expectSpecialFunctionCalls(catcherMocks[0], 1, -1, 1, 0);
    CachedValueWithTimeout<SomeType> value(
        [expiryTime, &timeShift, n = 0]() mutable
        {
            timeShift.applyRelativeShift(expiryTime + 1ms);
            return SomeType(n++);
        }, expiryTime);
    EXPECT_EQ(value.get().m_value, 0);

    expectSpecialFunctionCalls(catcherMocks[1], 1, -1, 2, 0);
    timeShift.applyRelativeShift(expiryTime + 1ms);
    EXPECT_EQ(value.get().m_value, 1);
    EXPECT_EQ(value.get().m_value, 1);
}

TEST(CachedValue, Concurrency)
{
    ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    ::testing::NiceMock<SpecialFunctionsCatcherMock> catcherMock;
    auto guard = nx::utils::makeScopeGuard(
        [](){ SomeType::m_catcher = &SomeType::m_defaultCatcher; });

    expectSpecialFunctionCalls(catcherMock, -1, -1, 2, 0);
    nx::Mutex mutex;
    CachedValue<SomeType> value(
        [&timeShift, &mutex]() mutable
        {
            NX_MUTEX_LOCKER lock(&mutex);
            timeShift.applyRelativeShift(std::chrono::milliseconds(1));
            return SomeType(0);
        });
    std::thread t1([&value](){ EXPECT_EQ(value.get().m_value, 0); });
    std::thread t2([&value](){ EXPECT_EQ(value.get().m_value, 0); });

    t1.join();
    t2.join();
}

// Concurrent get() callers should not all run the generator; the second one
// either returns the stale value (if any) or waits for the first.
TEST(CachedValue, StaleWhileRevalidate)
{
    std::promise<void> generatorStarted;
    std::promise<void> generatorMayContinue;
    std::atomic<int> generatorCallCount{0};

    constexpr std::chrono::milliseconds kShortTtl(1);
    ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    CachedValueWithTimeout<int> value(
        [&]()
        {
            const int call = ++generatorCallCount;
            if (call == 2)
            {
                generatorStarted.set_value();
                generatorMayContinue.get_future().wait();
            }
            return call;
        },
        kShortTtl);

    // Prime the cache.
    ASSERT_EQ(1, value.get());
    ASSERT_EQ(1, generatorCallCount);

    // Expire the timer so the next get() triggers a refresh.
    timeShift.applyRelativeShift(kShortTtl + 1ms);

    // Start a refreshing get() that will block in the generator.
    auto refreshing = std::async(std::launch::async, [&]() { return value.get(); });

    // Wait until the refreshing thread is inside the generator.
    generatorStarted.get_future().wait();

    // A concurrent get() now must return the stale value without waiting and
    // without starting a second generator.
    EXPECT_EQ(1, value.get());
    EXPECT_EQ(2, generatorCallCount);

    // Let the refreshing thread complete; it receives the freshly computed value.
    generatorMayContinue.set_value();
    EXPECT_EQ(2, refreshing.get());
    EXPECT_EQ(2, generatorCallCount);
}

// reset() while the generator is running must discard the result and force
// the next get() to run the generator again.
TEST(CachedValue, ResetDuringUpdate)
{
    std::promise<void> generatorStarted;
    std::promise<void> generatorMayContinue;
    std::atomic<int> generatorCallCount{0};

    CachedValueWithTimeout<int> value(
        [&]()
        {
            const int call = ++generatorCallCount;
            if (call == 1)
            {
                generatorStarted.set_value();
                generatorMayContinue.get_future().wait();
            }
            return call;
        },
        std::chrono::seconds(60));

    // Start a get() that will block in the first generator call.
    auto firstGet = std::async(std::launch::async, [&]() { return value.get(); });

    // Wait for the generator to actually be running.
    generatorStarted.get_future().wait();

    // reset() while the generator is in flight: its result must be discarded.
    value.reset();
    generatorMayContinue.set_value();

    // The first get() should now see its initial generator result discarded,
    // re-run the generator, and return the second value.
    EXPECT_EQ(2, firstGet.get());
    EXPECT_EQ(2, generatorCallCount);

    // Subsequent get() returns the cached value without rerunning.
    EXPECT_EQ(2, value.get());
    EXPECT_EQ(2, generatorCallCount);
}

// Several callers waiting for the very first value must all wake up with the
// same value once the lone generator finishes.
TEST(CachedValue, MultipleWaitersOnFirstValue)
{
    std::promise<void> generatorStarted;
    std::promise<void> generatorMayContinue;
    std::atomic<int> generatorCallCount{0};

    CachedValueWithTimeout<int> value(
        [&]()
        {
            ++generatorCallCount;
            generatorStarted.set_value();
            generatorMayContinue.get_future().wait();
            return 42;
        },
        std::chrono::seconds(60));

    constexpr int kWaiters = 5;
    std::vector<std::future<int>> futures;
    for (int i = 0; i < kWaiters; ++i)
        futures.push_back(std::async(std::launch::async, [&]() { return value.get(); }));

    // Make sure the generator started before letting it finish.
    generatorStarted.get_future().wait();
    generatorMayContinue.set_value();

    for (auto& f: futures)
        EXPECT_EQ(42, f.get());
    EXPECT_EQ(1, generatorCallCount);
}

} // namespace nx::utils::test
