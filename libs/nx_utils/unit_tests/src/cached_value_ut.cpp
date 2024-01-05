// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>
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
    CachedValue<SomeType> value([n = 0]() mutable { return SomeType(n++); }, expiryTime);
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

    // Update should reset timeout and update value.
    expectSpecialFunctionCalls(catcherMocks[4], 1, -1, 0, 0);
    value.update();

    // Make sure previous timeout didn't fire.
    expectSpecialFunctionCalls(catcherMocks[5], 0, -1, 1, 0);
    timeShift.applyRelativeShift(expiryTime / 2 + 1ms);
    EXPECT_EQ(value.get().m_value, 2);

    // Make sure value regenerated after timeout by update.
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
    CachedValue<SomeType> value(
        [expiryTime, &timeShift, n = 0]() mutable
        {
            timeShift.applyRelativeShift(expiryTime + 1ms);
            return SomeType(n++);
        }, expiryTime);
    EXPECT_EQ(value.updated().m_value, 0);

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

} // namespace nx::utils::test
