#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>
#include <nx/utils/value_cache.h>
#include <nx/utils/scope_guard.h>

namespace nx::utils::test {

class SpecialFunctionsCatcher
{
public:
    virtual ~SpecialFunctionsCatcher() = default;

    virtual void construct() = 0;
    virtual void destruct() = 0;
    virtual void copyConstruct() = 0;
    virtual void copyAssign() = 0;
};

class SpecialFunctionsNullCatcher : public SpecialFunctionsCatcher
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
    SomeType() { m_catcher->construct(); }
    ~SomeType() { m_catcher->destruct(); }

    SomeType(const SomeType&) { m_catcher->copyConstruct(); }
    SomeType& operator=(const SomeType&)
    {
        m_catcher->copyAssign();
        return *this;
    }

    SomeType(SomeType&&) = default;
    SomeType& operator=(SomeType&&) = default;
};

SpecialFunctionsNullCatcher SomeType::m_defaultCatcher;
SpecialFunctionsCatcher* SomeType::m_catcher = &SomeType::m_defaultCatcher;

class SpecialFunctionsCatcherMock : public SpecialFunctionsCatcher
{
public:
    MOCK_METHOD0(construct, void());
    MOCK_METHOD0(destruct, void());
    MOCK_METHOD0(copyConstruct, void());
    MOCK_METHOD0(copyAssign, void());
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

// TODO: test for an accuracy of timer resetting and expiary logic in a whole.
TEST(CachedValue, base)
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
     value.update();
     ASSERT_EQ(value.get(), 2);
}

TEST(CachedValue, constructionAndCopy)
{
    ::testing::NiceMock<SpecialFunctionsCatcherMock> catcherMocks[6];
    auto guard = nx::utils::makeScopeGuard(
        [](){ SomeType::m_catcher = &SomeType::m_defaultCatcher; });

    expectSpecialFunctionCalls(catcherMocks[0], 0, -1, -1, -1);
    CachedValue<SomeType> value([]() { return SomeType(); });

    expectSpecialFunctionCalls(catcherMocks[1], 1, -1, 1, 0);
    value.get();

    expectSpecialFunctionCalls(catcherMocks[2], 0, -1, 1, 0);
    value.get();

    expectSpecialFunctionCalls(catcherMocks[3], 1, 1, 0, 0);
    value.update();

    expectSpecialFunctionCalls(catcherMocks[4], 1, 1, 0, 0);
    value.update();

    expectSpecialFunctionCalls(catcherMocks[5], 0, 1, 0, 0);
    value.reset();
}

TEST(CachedValue, concurrency)
{
    ::testing::NiceMock<SpecialFunctionsCatcherMock> catcherMock;
    auto guard = nx::utils::makeScopeGuard(
        [](){ SomeType::m_catcher = &SomeType::m_defaultCatcher; });

    expectSpecialFunctionCalls(catcherMock, 1, -1, 2, 0);
    CachedValue<SomeType> value(
        []()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            return SomeType();
        });
    std::thread t1([&value](){ value.get(); });;
    std::thread t2([&value](){ value.get(); });

    t1.join();
    t2.join();
}

} // namespace nx::utils::test
