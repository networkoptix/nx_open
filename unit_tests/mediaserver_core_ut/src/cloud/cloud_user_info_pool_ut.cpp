#include <gtest/gtest.h>
#include <network/auth/cloud_user_info_pool.h>


TEST(CloudUserInfoPool, deserialize)
{
    const QString kSimpleString = lit("{\"timestamp\":1234567,\"cloudNonce\":\"abcdef0123456\"}");

    int64_t ts;
    nx::Buffer buf;
    detail::deserialize(kSimpleString, &ts, &buf);


    ASSERT_EQ(ts, 1234567);
    ASSERT_EQ(buf, "abcdef0123456");
}

namespace test  {

class TestPoolSupplier : public AbstractCloudUserInfoPoolSupplier
{
public:
    virtual void setPool(AbstractCloudUserInfoPool* pool) override
    {
        m_pool = pool;
    }

    void setUserInfo(int64_t ts, const nx::Buffer& userName, const nx::Buffer& cloudNonce)
    {
        m_pool->userInfoChanged(ts, userName, cloudNonce);
    }

    void removeUserInfo(const nx::Buffer& userName)
    {
        m_pool->userInfoRemoved(userName);
    }

private:
    AbstractCloudUserInfoPool* m_pool;
};

class Pool : public ::testing::Test
{
protected:
    Pool(): 
        supplier(new TestPoolSupplier),
        userInfoPool(std::unique_ptr<AbstractCloudUserInfoPoolSupplier>(supplier)) {}

    TestPoolSupplier* supplier;
    CloudUserInfoPool userInfoPool;
};

TEST_F(Pool, main)
{
    supplier->setUserInfo(1, "vasya", "nonce1");
    supplier->setUserInfo(2, "vasya", "nonce2");
    supplier->setUserInfo(2, "petya", "nonce2");
    supplier->setUserInfo(3, "petya", "nonce3");

    auto nonce = userInfoPool.newestMostCommonNonce();
    ASSERT_TRUE(nonce);
    ASSERT_EQ(nonce, "nonce2");
}

}

