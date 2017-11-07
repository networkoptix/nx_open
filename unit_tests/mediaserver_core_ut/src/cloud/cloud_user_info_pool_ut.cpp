#include <gtest/gtest.h>

#include <nx/network/http/auth_tools.h>

#include <nx/vms/cloud_integration/cloud_user_info_pool.h>
#include <nx/vms/cloud_integration/cdb_nonce_fetcher.h>

namespace test  {

static const nx::Buffer kTestRealm = "test-realm";
static const nx::Buffer kTestAlgorithm = "MD5";
static const nx::Buffer kTestPassword = "password";
static const nx::Buffer kWrongPassword = "wrong_password";
static const nx::Buffer kTestUri = "/uri";
static const nx::Buffer kTestMethod = "GET";

class TestCloudUserInfoPool:
    public nx::vms::cloud_integration::CloudUserInfoPool
{
public:
    using CloudUserInfoPool::CloudUserInfoPool;
};

class TestSupplier:
    public nx::vms::cloud_integration::AbstractCloudUserInfoPoolSupplier
{
public:
    virtual void setPool(nx::vms::cloud_integration::AbstractCloudUserInfoPool* pool)
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
    nx::vms::cloud_integration::AbstractCloudUserInfoPool* m_pool;
};

class CloudUserInfoPool: public ::testing::Test
{
protected:
    CloudUserInfoPool():
        supplier(new TestSupplier),
        userInfoPool(std::unique_ptr<nx::vms::cloud_integration::AbstractCloudUserInfoPoolSupplier>(supplier))
    {}

    nx_http::header::Authorization generateAuthHeader(
        const nx::Buffer& userName,
        const nx::Buffer& cloudNonce,
        const nx::Buffer& password = kTestPassword)
    {
        nx_http::header::WWWAuthenticate authServerHeader;
        authServerHeader.authScheme = nx_http::header::AuthScheme::digest;
        authServerHeader.params["nonce"] = cloudNonce + nx::vms::cloud_integration::CdbNonceFetcher::generateNonceTrailer();
        authServerHeader.params["realm"] = kTestRealm;
        authServerHeader.params["algorithm"] = kTestAlgorithm;

        nx_http::header::DigestAuthorization authClientResponseHeader;
        authClientResponseHeader.digest->userid = userName;
        nx_http::calcDigestResponse(
            "GET",
            userName,
            password,
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


    void thenCommonNonceShouldBeNull()
    {
        ASSERT_FALSE((bool)userInfoPool.newestMostCommonNonce());
    }

    void thenSecondStillCanAuth()
    {
        ASSERT_EQ(Qn::Auth_WrongLogin, userInfoPool.authenticate(
            kTestMethod,
            generateAuthHeader("vasya", *userInfoPool.newestMostCommonNonce())));

        ASSERT_EQ(Qn::Auth_OK, userInfoPool.authenticate(
            kTestMethod,
            generateAuthHeader("petya", *userInfoPool.newestMostCommonNonce())));
    }

    void thenBothUsersCanAuthenticateUsingNewestMostCommonNonce()
    {
        ASSERT_EQ(Qn::Auth_OK, userInfoPool.authenticate(
            kTestMethod,
            generateAuthHeader("vasya", *userInfoPool.newestMostCommonNonce())));

        ASSERT_EQ(Qn::Auth_OK, userInfoPool.authenticate(
            kTestMethod,
            generateAuthHeader("petya", *userInfoPool.newestMostCommonNonce())));
    }

    void thenBothUsersCanNotAuthenticateWithWrongPasswords()
    {
        ASSERT_EQ(Qn::Auth_WrongPassword, userInfoPool.authenticate(
            kTestMethod,
            generateAuthHeader("vasya", *userInfoPool.newestMostCommonNonce(), kWrongPassword)));

        ASSERT_EQ(Qn::Auth_WrongPassword, userInfoPool.authenticate(
            kTestMethod,
            generateAuthHeader("vasya", *userInfoPool.newestMostCommonNonce(), kWrongPassword)));
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

TEST_F(CloudUserInfoPool, IncorrectPassword)
{
    given2UsersInfosWithCommonFirstNonce();
    thenBothUsersCanNotAuthenticateWithWrongPasswords();
}

} // namespace test
