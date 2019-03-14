#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace nx {
namespace test {

struct TestInterface
{
    virtual ~TestInterface() {}

    virtual int add(int a, int b) = 0;
    virtual int dev(int a, int b) = 0;
};

struct TestImpl : TestInterface
{
    virtual int add(int a, int b) { return a + b; }
    virtual int dev(int a, int b) { return a / b; }
};

struct TestMock : TestInterface
{
    MOCK_METHOD2(add, int(int a, int b));
    MOCK_METHOD2(dev, int(int a, int b));
};

static int avg(TestInterface& executor, int a, int b)
{
    return executor.dev(executor.add(a, b), 2);
}

TEST(GmockExample, Impl)
{
    TestImpl impl;
    EXPECT_EQ(avg(impl, 5, 3), 4);
}

TEST(GmockExample, Mock)
{
    TestMock mock;

    using ::testing::Return;
    EXPECT_CALL(mock, add(5, 3)).Times(1).WillOnce(Return(8)); // 5 + 3 = 8
    EXPECT_CALL(mock, dev(8, 2)).Times(1).WillOnce(Return(4)); // 8 / 2 = 4

    EXPECT_EQ(avg(mock, 5, 3), 4);
}


} // namespace test
} // namespace nx
