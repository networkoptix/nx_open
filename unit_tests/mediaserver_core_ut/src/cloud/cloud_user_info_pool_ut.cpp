#include <gtest/gtest.h>
#include <network/auth/cloud_user_info_pool.h>
#include <network/auth/cdb_nonce_fetcher.h>
#include <nx/network/http/auth_tools.h>


namespace test  {

const nx::Buffer kTestRealm = "test-realm";
const nx::Buffer kTestAlgorithm = "MD5";
const nx::Buffer kTestPassword = "password";
const nx::Buffer kTestUri = "/uri";
const nx::Buffer kTestMethod = "GET";


class TestCdbNonceFetcher : public ::CdbNonceFetcher
{
public:
    static nx::Buffer generateNonceTrailer()
    {
        return CdbNonceFetcher::generateNonceTrailer([]() { return 1; });
    }
};

class TestPoolSupplier : public AbstractCloudUserInfoPoolSupplier
{
public:
    TestPoolSupplier() {}
    virtual void setPool(AbstractCloudUserInfoPool* pool) override
    {
        m_pool = pool;
    }

    nx_http::header::Authorization setUserInfo(
        uint64_t ts,
        const nx::Buffer& userName,
        const nx::Buffer& cloudNonce)
    {
        nx_http::header::WWWAuthenticate authServerHeader;
        authServerHeader.authScheme = nx_http::header::AuthScheme::digest;
        authServerHeader.params["nonce"] = cloudNonce + TestCdbNonceFetcher::generateNonceTrailer();
        authServerHeader.params["realm"] = kTestRealm;
        authServerHeader.params["algorithm"] = kTestAlgorithm;

        nx_http::header::DigestAuthorization authClientResponseHeader;
        authClientResponseHeader.digest->userid = userName;
        nx_http::calcDigestResponse(
            "GET",
            userName,
            kTestPassword,
            boost::none,
            kTestUri,
            authServerHeader,
            &authClientResponseHeader);

        const auto ha1 = nx_http::calcHa1(userName, kTestRealm, kTestPassword, kTestAlgorithm);
        const auto partialResponse = nx_http::calcIntermediateResponse(ha1, cloudNonce);
        m_pool->userInfoChanged(ts, userName, cloudNonce, partialResponse);

        return authClientResponseHeader;
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
        userInfoPool(std::unique_ptr<AbstractCloudUserInfoPoolSupplier>(supplier))
    {}

    void given2UsersInfos()
    {
        vasya1Auth = supplier->setUserInfo(1, "vasya", generateTestNonce(1));
        vasya2Auth = supplier->setUserInfo(2, "vasya", generateTestNonce(2));
        petya2Auth = supplier->setUserInfo(2, "petya", generateTestNonce(2));
        petya3Auth = supplier->setUserInfo(3, "petya", generateTestNonce(3));
    }

    void when3rdWithNonce3HasBeenAdded()
    {
        gena3Auth = supplier->setUserInfo(3, "gena", generateTestNonce(3));
    }

    void whenVasyaNonceUpdatedToTheNewest()
    {
        vasya3Auth = supplier->setUserInfo(3, "vasya", generateTestNonce(3));
    }

    void thenOnlyNewestNonceShouldStayInThePool()
    {
        auto& vasyaNonceSet = userInfoPool.nameToNonces().find("vasya")->second;
        ASSERT_EQ(vasyaNonceSet.find("nonce1"), vasyaNonceSet.cend());
        ASSERT_EQ(vasyaNonceSet.find("nonce2"), vasyaNonceSet.cend());

        /*auto& petyaNonceSet =*/ userInfoPool.nameToNonces().find("petya")->second;
        ASSERT_EQ(vasyaNonceSet.find("nonce2"), vasyaNonceSet.cend());
    }

    nx::Buffer generateTestNonce(char c)
    {
        nx::Buffer result;
        result.fill(c, 95);
        return result;
    }

    TestPoolSupplier* supplier;
    TestCloudUserInfoPool userInfoPool;
    nx_http::header::Authorization vasya1Auth;
    nx_http::header::Authorization vasya2Auth;
    nx_http::header::Authorization petya2Auth;
    nx_http::header::Authorization petya3Auth;
    nx_http::header::Authorization gena3Auth;
    nx_http::header::Authorization vasya3Auth;
};

TEST_F(CloudUserInfoPool, deserialize)
{
    const QString kSimpleString =
        lm("{\"timestamp\":1234567,\"cloudNonce\":\"abcdef0123456\",\"partialResponse\":\"response\"}");

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
    ASSERT_TRUE((bool)nonce);
    ASSERT_EQ(*nonce, nx::Buffer(generateTestNonce(2)));
}

TEST_F(CloudUserInfoPool, commonNonce_3rdAdded)
{
    given2UsersInfos();
    when3rdWithNonce3HasBeenAdded();

    auto nonce = userInfoPool.newestMostCommonNonce();
    ASSERT_TRUE((bool)nonce);
    ASSERT_EQ(*nonce, nx::Buffer(generateTestNonce(3)));
}

TEST_F(CloudUserInfoPool, commonNonce_EveryoneHaveNewestNonce)
{
    given2UsersInfos();
    when3rdWithNonce3HasBeenAdded();
    whenVasyaNonceUpdatedToTheNewest();
    thenOnlyNewestNonceShouldStayInThePool();

    auto nonce = userInfoPool.newestMostCommonNonce();
    ASSERT_TRUE((bool)nonce);
    ASSERT_EQ(*nonce, nx::Buffer(generateTestNonce(3)));
}

TEST_F(CloudUserInfoPool, auth_2Users)
{
    given2UsersInfos();
    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, vasya2Auth));
    // old nonce user info should have already been deleted
    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya1Auth));

    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya2Auth));
    // But newer user info should be in the pool
    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya3Auth));
}

TEST_F(CloudUserInfoPool, auth_3Users)
{
    given2UsersInfos();
    when3rdWithNonce3HasBeenAdded();

    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, vasya2Auth));
    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya2Auth));
    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya3Auth));
    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, gena3Auth));
}

TEST_F(CloudUserInfoPool, auth_3Users_newNonceAll)
{
    given2UsersInfos();
    when3rdWithNonce3HasBeenAdded();
    whenVasyaNonceUpdatedToTheNewest();

    // old nonces should have been removed
    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya1Auth));
    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya2Auth));
    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, petya2Auth));

    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya3Auth));
    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, gena3Auth));
    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, vasya3Auth));
}

TEST_F(CloudUserInfoPool, userRemoved)
{
    given2UsersInfos();
    supplier->removeUserInfo("vasya");

    auto nonce = userInfoPool.newestMostCommonNonce();
    ASSERT_TRUE((bool)nonce);
    ASSERT_EQ(*nonce, nx::Buffer(generateTestNonce(3)));

    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya1Auth));
    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya2Auth));

    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya2Auth));
    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya3Auth));
}

TEST_F(CloudUserInfoPool, userRemoved_3users)
{
    given2UsersInfos();
    when3rdWithNonce3HasBeenAdded();
    whenVasyaNonceUpdatedToTheNewest();

    supplier->removeUserInfo("vasya");

    auto nonce = userInfoPool.newestMostCommonNonce();
    ASSERT_TRUE((bool)nonce);
    ASSERT_EQ(*nonce, nx::Buffer(generateTestNonce(3)));

    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya1Auth));
    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya2Auth));
    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya3Auth));
    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, petya2Auth));

    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya3Auth));
    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, gena3Auth));
}

}

