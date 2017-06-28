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

    void removeUser(const nx::Buffer& name)
    {
        m_pool->userInfoRemoved(name);
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

    void andWhenSecondUserNonceInfoIsUpdated()
    {
        nx::cdb::api::AuthInfo petyaAuthInfo;

        auto petyaNonce1 = generateTestNonce(1);
        auto petya = "petya";
        petyaAuthInfo.records.push_back(
            nx::cdb::api::AuthInfoRecord(
                petyaNonce1.toStdString(),
                generateIntermediateResponse(petya, petyaNonce1).toStdString(),
                std::chrono::system_clock::time_point(std::chrono::milliseconds(2))));

        auto petyaNonce2 = generateTestNonce(2);
        petyaAuthInfo.records.push_back(
            nx::cdb::api::AuthInfoRecord(
                petyaNonce2.toStdString(),
                generateIntermediateResponse(petya, petyaNonce2).toStdString(),
                std::chrono::system_clock::time_point(std::chrono::milliseconds(12))));

        supplier->setUserInfo(petya, petyaAuthInfo);
    }

    void whenFirstUserIsRemoved()
    {
        supplier->removeUser("vasya");
    }

    void whenAllUsersRemoved()
    {
        supplier->removeUser("vasya");
        supplier->removeUser("petya");
    }

    void whenClearCalled()
    {
        userInfoPool.clear();
    }


    void thenCommonNonceShouldBeNull ()
    {
        ASSERT_FALSE((bool)userInfoPool.newestMostCommonNonce());
    }

    void thenSecondStillCanAuth()
    {
        ASSERT_FALSE(userInfoPool.authenticate(
            kTestMethod,
            generateAuthHeader("vasya", *userInfoPool.newestMostCommonNonce())));

        ASSERT_TRUE(userInfoPool.authenticate(
            kTestMethod,
            generateAuthHeader("petya", *userInfoPool.newestMostCommonNonce())));
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

TEST_F(CloudUserInfoPool, commonNonce_twoUsers_bothNonceAreCommon)
{
    given2UsersInfosWithCommonFirstNonce();
    andWhenSecondUserNonceInfoIsUpdated();
    thenBothUsersCanAuthenticateUsingNewestMostCommonNonce();
}

TEST_F(CloudUserInfoPool, commonNonce_twoUsers_onlyFirstNonceIsCommon_SecondRemoved)
{
    given2UsersInfosWithCommonFirstNonce();
    whenFirstUserIsRemoved();
    thenSecondStillCanAuth();
}

TEST_F(CloudUserInfoPool, allUsersRemoved_nonceCleared)
{
    given2UsersInfosWithCommonFirstNonce();
    whenAllUsersRemoved();
    thenCommonNonceShouldBeNull();
}

TEST_F(CloudUserInfoPool, clear)
{
    given2UsersInfosWithCommonFirstNonce();
    whenClearCalled();
    thenCommonNonceShouldBeNull();
}

}

