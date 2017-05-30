#include <gtest/gtest.h>
#include <network/auth/cloud_user_info_pool.h>


namespace test  {

class TestPoolSupplier : public AbstractCloudUserInfoPoolSupplier
{
public:
    virtual void setPool(AbstractCloudUserInfoPool* pool) override
    {
        m_pool = pool;
    }

    void setUserInfo(
        uint64_t ts,
        const nx::Buffer& userName,
        const nx::Buffer& cloudNonce)
    {
        m_pool->userInfoChanged(ts, userName, cloudNonce, partialResponse);
    }

    void removeUserInfo(const nx::Buffer& userName)
    {
        m_pool->userInfoRemoved(userName);
    }

private:
    AbstractCloudUserInfoPool* m_pool;
};

class TestCloudUserInfoPool : public ::CloudUserInfoPool
{
public:
    using CloudUserInfoPool::CloudUserInfoPool;
    detail::UserNonceToResponseMap& userNonceToResponse() { return m_userNonceToResponse; }
    detail::TimestampToNonceUserCountMap& tsNonce() { return m_tsToNonceUserCount; }
    detail::NameToNoncesMap& nameToNonces() { return m_nameToNonces; }
    detail::NonceToTsMap& nonceToTs() { return m_nonceToTs; }
    int userCount() { return m_userCount; }
};

class CloudUserInfoPool: public ::testing::Test
{
protected:
    CloudUserInfoPool():
        supplier(new TestPoolSupplier),
        userInfoPool(std::unique_ptr<AbstractCloudUserInfoPoolSupplier>(supplier)),
        authHeader(nx_http::header::AuthScheme::digest)
    {}

    void given2UsersInfos()
    {
        supplier->setUserInfo(1, "vasya", "nonce1", "responseVasya1");
        supplier->setUserInfo(2, "vasya", "nonce2", "responseVasya2");
        supplier->setUserInfo(2, "petya", "nonce2", "responsePetya1");
        supplier->setUserInfo(3, "petya", "nonce3", "responsePetya2");
    }

    void givenVasyaNonce1AuthHeader()
    {
        authHeader.digest->userid = "vasya";
        authHeader.digest->params["nonce"] = "nonce1";
    }

    void when3rdWithNonce3HasBeenAdded()
    {
        supplier->setUserInfo(3, "gena", "nonce3", "responseGena3");
    }

    void whenVasyaNonceUpdatedToTheNewest()
    {
        supplier->setUserInfo(3, "vasya", "nonce3", "responseVasya3");
    }

    void thenOnlyNewestNonceShouldStayInThePool()
    {
        auto& vasyaNonceSet = userInfoPool.nameToNonces().find("vasya")->second;
        ASSERT_EQ(vasyaNonceSet.find("nonce1"), vasyaNonceSet.cend());
        ASSERT_EQ(vasyaNonceSet.find("nonce2"), vasyaNonceSet.cend());

        auto& petyaNonceSet = userInfoPool.nameToNonces().find("petya")->second;
        ASSERT_EQ(vasyaNonceSet.find("nonce2"), vasyaNonceSet.cend());
    }

    TestPoolSupplier* supplier;
    TestCloudUserInfoPool userInfoPool;
    nx_http::header::Authorization authHeader;
};

TEST_F(CloudUserInfoPool, deserialize)
{
    const QString kSimpleString =
        lit("{\"timestamp\":1234567,\"cloudNonce\":\"abcdef0123456\",\"partialResponse\":\"response\"}");

    uint64_t ts;
    nx::Buffer nonce;
    nx::Buffer response;
    ASSERT_TRUE(detail::deserialize(kSimpleString, &ts, &nonce, &response));


    ASSERT_EQ(ts, 1234567);
    ASSERT_EQ(nonce, "abcdef0123456");
    ASSERT_EQ(response, "response");
}


TEST_F(CloudUserInfoPool, commonNonce_simple)
{
    given2UsersInfos();
    auto nonce = userInfoPool.newestMostCommonNonce();
    ASSERT_TRUE(nonce);
    ASSERT_EQ(*nonce, nx::Buffer("nonce2"));
}

TEST_F(CloudUserInfoPool, commonNonce_3rdAdded)
{
    given2UsersInfos();
    when3rdWithNonce3HasBeenAdded();

    auto nonce = userInfoPool.newestMostCommonNonce();
    ASSERT_TRUE(nonce);
    ASSERT_EQ(*nonce, nx::Buffer("nonce3"));
}

TEST_F(CloudUserInfoPool, commonNonce_EveryoneHaveNewestNonce)
{
    given2UsersInfos();
    when3rdWithNonce3HasBeenAdded();
    whenVasyaNonceUpdatedToTheNewest();
    thenOnlyNewestNonceShouldStayInThePool();

    auto nonce = userInfoPool.newestMostCommonNonce();
    ASSERT_TRUE(nonce);
    ASSERT_EQ(*nonce, nx::Buffer("nonce3"));
}

TEST_F(CloudUserInfoPool, auth)
{
    given2UsersInfos();
}

}

