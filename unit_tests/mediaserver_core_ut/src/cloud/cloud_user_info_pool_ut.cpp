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

class TestCloudUserInfoPool : public ::CloudUserInfoPool
{
public:
    using CloudUserInfoPool::CloudUserInfoPool;
};

class TestSupplier: public AbstractCloudUserInfoPoolSupplier
{
public:
    virtual void setPool(AbstractCloudUserInfoPool* pool)
    {
        m_pool = pool;
    }

    void setUserInfo(const nx::Buffer& name, const nx::cdb::api::AuthInfo& info)
    {
        m_pool->userInfoChanged(name, info);
    }

private:
    AbstractCloudUserInfoPool* m_pool;
};

class CloudUserInfoPool: public ::testing::Test
{
protected:
    CloudUserInfoPool():
        supplier(new TestSupplier),
        userInfoPool(std::unique_ptr<AbstractCloudUserInfoPoolSupplier>(supplier))
    {}

    nx_http::header::Authorization generateAuthHeader(
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

        return authClientResponseHeader;
    }

    nx::Buffer generateIntermediateResponse(const nx::Buffer& userName, const nx::Buffer& nonce)
    {
        const auto ha1 = nx_http::calcHa1(userName, kTestRealm, kTestPassword, kTestAlgorithm);
        return nx_http::calcIntermediateResponse(ha1, nonce);
    }
    void given2UsersInfosWithCommonFirstNonce()
    {
        nx::cdb::api::AuthInfo vasyaAuthInfo;

        auto vasya = "vasya";
        auto vasyaNonce1 = generateTestNonce(1);
        vasyaAuthInfo.records.push_back(
            nx::cdb::api::AuthInfoRecord(
                vasyaNonce1.toStdString(),
                generateIntermediateResponse(vasya, vasyaNonce1).toStdString(),
                std::chrono::system_clock::time_point(std::chrono::milliseconds(1))));

        auto vasyaNonce2 = generateTestNonce(2);
        vasyaAuthInfo.records.push_back(
            nx::cdb::api::AuthInfoRecord(
                vasyaNonce2.toStdString(),
                generateIntermediateResponse(vasya, vasyaNonce2).toStdString(),
                std::chrono::system_clock::time_point(std::chrono::milliseconds(5))));

        nx::cdb::api::AuthInfo petyaAuthInfo;

        auto petyaNonce1 = vasyaNonce1;
        auto petya = "petya";
        petyaAuthInfo.records.push_back(
            nx::cdb::api::AuthInfoRecord(
                petyaNonce1.toStdString(),
                generateIntermediateResponse(petya, petyaNonce1).toStdString(),
                std::chrono::system_clock::time_point(std::chrono::milliseconds(2))));

        supplier->setUserInfo(vasya, vasyaAuthInfo);
        supplier->setUserInfo(petya, petyaAuthInfo);
    }

    void thenBothUsersCanAuthenticateUsingNewestMostCommonNonce()
    {
        ASSERT_TRUE(userInfoPool.authenticate(
            kTestMethod,
            generateAuthHeader("vasya", *userInfoPool.newestMostCommonNonce())));

        ASSERT_TRUE(userInfoPool.authenticate(
            kTestMethod,
            generateAuthHeader("petya", *userInfoPool.newestMostCommonNonce())));
    }
    //void whenSameUserIsReported()
    //{
    //    vasya1Auth = supplier->setUserInfo(2, "petya", generateTestNonce(2));
    //}

    //void when3rdWithNonce3HasBeenAdded()
    //{
    //    gena3Auth = supplier->setUserInfo(3, "gena", generateTestNonce(3));
    //}

    //void whenVasyaNonceUpdatedToTheNewest()
    //{
    //    vasya3Auth = supplier->setUserInfo(3, "vasya", generateTestNonce(3));
    //}

    //void thenOnlyNewestNonceShouldStayInThePool()
    //{
    //    ASSERT_TRUE(false);
    //}

    nx::Buffer generateTestNonce(char c)
    {
        nx::Buffer result;
        result.fill(c, 95);
        return result;
    }

    TestSupplier* supplier;
    TestCloudUserInfoPool userInfoPool;
    std::map<nx::Buffer, nx_http::header::Authorization> userToAuthHeader;
};

TEST_F(CloudUserInfoPool, commonNonce_twoUsers_onlyFirstNonceIsCommon)
{
    given2UsersInfosWithCommonFirstNonce();
    thenBothUsersCanAuthenticateUsingNewestMostCommonNonce();
}

//TEST_F(CloudUserInfoPool, sameUserReportedTwice)
//{
//    given2UsersInfos();
//    whenSameUserIsReported();
//
//    auto nonce = userInfoPool.newestMostCommonNonce();
//    ASSERT_TRUE((bool)nonce);
//    ASSERT_EQ(*nonce, nx::Buffer(generateTestNonce(2)));
//}
//
//TEST_F(CloudUserInfoPool, commonNonce_3rdAdded)
//{
//    given2UsersInfos();
//    when3rdWithNonce3HasBeenAdded();
//
//    auto nonce = userInfoPool.newestMostCommonNonce();
//    ASSERT_TRUE((bool)nonce);
//    ASSERT_EQ(*nonce, nx::Buffer(generateTestNonce(3)));
//}
//
//TEST_F(CloudUserInfoPool, commonNonce_EveryoneHaveNewestNonce)
//{
//    given2UsersInfos();
//    when3rdWithNonce3HasBeenAdded();
//    whenVasyaNonceUpdatedToTheNewest();
//    thenOnlyNewestNonceShouldStayInThePool();
//
//    auto nonce = userInfoPool.newestMostCommonNonce();
//    ASSERT_TRUE((bool)nonce);
//    ASSERT_EQ(*nonce, nx::Buffer(generateTestNonce(3)));
//}
//
//TEST_F(CloudUserInfoPool, auth_2Users)
//{
//    given2UsersInfos();
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, vasya2Auth));
//    // old nonce user info should have already been deleted
//    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya1Auth));
//
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya2Auth));
//    // But newer user info should be in the pool
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya3Auth));
//}
//
//TEST_F(CloudUserInfoPool, auth_3Users)
//{
//    given2UsersInfos();
//    when3rdWithNonce3HasBeenAdded();
//
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, vasya2Auth));
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya2Auth));
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya3Auth));
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, gena3Auth));
//}
//
//TEST_F(CloudUserInfoPool, auth_3Users_newNonceAll)
//{
//    given2UsersInfos();
//    when3rdWithNonce3HasBeenAdded();
//    whenVasyaNonceUpdatedToTheNewest();
//
//    // old nonces should have been removed
//    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya1Auth));
//    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya2Auth));
//    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, petya2Auth));
//
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya3Auth));
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, gena3Auth));
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, vasya3Auth));
//}
//
//TEST_F(CloudUserInfoPool, userRemoved)
//{
//    given2UsersInfos();
//    supplier->removeUserInfo("vasya");
//
//    auto nonce = userInfoPool.newestMostCommonNonce();
//    ASSERT_TRUE((bool)nonce);
//    ASSERT_EQ(*nonce, nx::Buffer(generateTestNonce(3)));
//
//    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya1Auth));
//    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya2Auth));
//
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya2Auth));
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya3Auth));
//}
//
//TEST_F(CloudUserInfoPool, userRemoved_3users)
//{
//    given2UsersInfos();
//    when3rdWithNonce3HasBeenAdded();
//    whenVasyaNonceUpdatedToTheNewest();
//
//    supplier->removeUserInfo("vasya");
//
//    auto nonce = userInfoPool.newestMostCommonNonce();
//    ASSERT_TRUE((bool)nonce);
//    ASSERT_EQ(*nonce, nx::Buffer(generateTestNonce(3)));
//
//    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya1Auth));
//    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya2Auth));
//    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, vasya3Auth));
//    ASSERT_FALSE(userInfoPool.authenticate(kTestMethod, petya2Auth));
//
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, petya3Auth));
//    ASSERT_TRUE(userInfoPool.authenticate(kTestMethod, gena3Auth));
//}

}

