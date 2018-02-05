#include <boost/optional.hpp>

#include <gtest/gtest.h>

#include <nx/utils/time.h>

#include <nx/cloud/cdb/dao/rdb/dao_rdb_user_authentication.h>
#include <nx/cloud/cdb/dao/memory/dao_memory_user_authentication.h>

#include "base_persistent_data_test.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {
namespace test {

using BasePersistentDataTest = cdb::test::BasePersistentDataTest;

template<typename DaoType>
class DaoUserAuthentication:
    public ::testing::Test,
    public BasePersistentDataTest
{
public:
    DaoUserAuthentication()
    {
        m_ownerAccount = insertRandomAccount();
        m_system = insertRandomSystem(m_ownerAccount);
        prepareTestData();
    }

protected:
    void givenAccountWithMultipleSystems()
    {
        using namespace std::placeholders;

        for (int i = 0; i < 7; ++i)
        {
            const auto system = insertRandomSystem(m_ownerAccount);
            auto authInfo = generateAuthInfo();

            AbstractUserAuthentication::SystemInfo systemInfo;
            systemInfo.systemId = system.id;
            systemInfo.nonce = authInfo.records[0].nonce;
            //systemInfo.vmsUserId = ;
            m_expectedSystemInfo.emplace(system.id, systemInfo);

            executeUpdateQuerySyncThrow(
                std::bind(&dao::AbstractUserAuthentication::insertUserAuthRecords,
                    &m_dao, _1, system.id, m_ownerAccount.id, authInfo));
            executeUpdateQuerySyncThrow(
                std::bind(&dao::AbstractUserAuthentication::insertOrReplaceSystemNonce,
                    &m_dao, _1, system.id, authInfo.records[0].nonce));
        }
    }

    void whenAddedAuthRecord()
    {
        using namespace std::placeholders;

        executeUpdateQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::insertUserAuthRecords,
                &m_dao, _1, m_system.id, m_ownerAccount.id, m_expectedAuthInfo));
    }

    void whenFetchedAuthRecords()
    {
        using namespace std::placeholders;

        m_fetchedAuthInfo = executeSelectQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::fetchUserAuthRecords,
                &m_dao, _1, m_system.id, m_ownerAccount.id));
    }

    void whenInsertedSystemNonce()
    {
        using namespace std::placeholders;

        m_expectedNonce = nx::utils::generateRandomName(12).toStdString();

        executeUpdateQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::insertOrReplaceSystemNonce,
                &m_dao, _1, m_system.id, m_expectedNonce));
    }

    void whenReplacedExistingNonce()
    {
        whenInsertedSystemNonce();
        whenInsertedSystemNonce();
    }

    void whenFetchedSystemNonce()
    {
        using namespace std::placeholders;

        m_fetchedNonce = executeSelectQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::fetchSystemNonce,
                &m_dao, _1, m_system.id));
    }

    void whenDeleteAllRecords()
    {
        using namespace std::placeholders;

        executeUpdateQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::deleteAccountAuthRecords,
                &m_dao, _1, m_ownerAccount.id));
    }

    void thenAuthRecordCanBeRead()
    {
        whenFetchedAuthRecords();
        ASSERT_EQ(m_expectedAuthInfo, m_fetchedAuthInfo);
    }

    void thenRecordListIsEmpty()
    {
        ASSERT_TRUE(m_fetchedAuthInfo.records.empty());
    }

    void thenNonceCanBeFetched()
    {
        whenFetchedSystemNonce();
        ASSERT_EQ(m_expectedNonce, *m_fetchedNonce);
    }

    void thenNoNonceFound()
    {
        ASSERT_FALSE(static_cast<bool>(m_fetchedNonce));
    }

    void thenEverySystemInformationCanBeSelected()
    {
        using namespace std::placeholders;

        auto systems = executeSelectQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::fetchAccountSystems,
                &m_dao, _1, m_ownerAccount.id));

        // Validating.
        ASSERT_EQ(m_expectedSystemInfo.size(), systems.size());
        for (const auto& system: systems)
        {
            auto it = m_expectedSystemInfo.find(system.systemId);
            ASSERT_NE(m_expectedSystemInfo.end(), it);
            ASSERT_EQ(it->second.nonce, system.nonce);
            //ASSERT_EQ(it->second.vmsUserId, system.vmsUserId);
        }
    }

    void thenAllRecordsHaveBeenDeleted()
    {
        using namespace std::placeholders;

        auto systems = executeSelectQuerySyncThrow(
            std::bind(&dao::AbstractUserAuthentication::fetchAccountSystems,
                &m_dao, _1, m_ownerAccount.id));
        for (const auto& system: systems)
        {
            auto fetchedAuthInfo = executeSelectQuerySyncThrow(
                std::bind(&dao::AbstractUserAuthentication::fetchUserAuthRecords,
                    &m_dao, _1, system.systemId, m_ownerAccount.id));
            ASSERT_TRUE(fetchedAuthInfo.records.empty());
        }
    }

private:
    DaoType m_dao;
    api::AccountData m_ownerAccount;
    api::SystemData m_system;
    api::AuthInfo m_expectedAuthInfo;
    api::AuthInfo m_fetchedAuthInfo;
    std::string m_expectedNonce;
    boost::optional<std::string> m_fetchedNonce;
    std::map<std::string, AbstractUserAuthentication::SystemInfo> m_expectedSystemInfo;

    void prepareTestData()
    {
        m_expectedAuthInfo = generateAuthInfo();
    }

    api::AuthInfo generateAuthInfo()
    {
        api::AuthInfo authInfo;
        authInfo.records.resize(1);
        authInfo.records[0].expirationTime =
            nx::utils::floor<std::chrono::milliseconds>(nx::utils::utcTime());
        authInfo.records[0].nonce = nx::utils::generateRandomName(7).toStdString();
        authInfo.records[0].intermediateResponse =
            nx::utils::generateRandomName(14).toStdString();
        return authInfo;
    }
};

TYPED_TEST_CASE_P(DaoUserAuthentication);

//-------------------------------------------------------------------------------------------------
// Test cases.

TYPED_TEST_P(DaoUserAuthentication, saved_user_auth_records_can_be_read_later)
{
    this->whenAddedAuthRecord();
    this->thenAuthRecordCanBeRead();
}

TYPED_TEST_P(DaoUserAuthentication, fetching_empty_auth_record_list)
{
    this->whenFetchedAuthRecords();
    this->thenRecordListIsEmpty();
}

TYPED_TEST_P(DaoUserAuthentication, inserting_system_nonce)
{
    this->whenInsertedSystemNonce();
    this->thenNonceCanBeFetched();
}

TYPED_TEST_P(DaoUserAuthentication, fetching_not_existing_nonce)
{
    this->whenFetchedSystemNonce();
    this->thenNoNonceFound();
}

TYPED_TEST_P(DaoUserAuthentication, replacing_system_nonce)
{
    this->whenReplacedExistingNonce();
    this->thenNonceCanBeFetched();
}

TYPED_TEST_P(DaoUserAuthentication, fetching_every_system_of_an_account)
{
    this->givenAccountWithMultipleSystems();
    this->thenEverySystemInformationCanBeSelected();
}

TYPED_TEST_P(DaoUserAuthentication, deleting_every_auth_record_of_an_account)
{
    this->givenAccountWithMultipleSystems();
    this->whenDeleteAllRecords();
    this->thenAllRecordsHaveBeenDeleted();
}

REGISTER_TYPED_TEST_CASE_P(DaoUserAuthentication,
    saved_user_auth_records_can_be_read_later,
    fetching_empty_auth_record_list,
    inserting_system_nonce,
    fetching_not_existing_nonce,
    replacing_system_nonce,
    fetching_every_system_of_an_account,
    deleting_every_auth_record_of_an_account);

//-------------------------------------------------------------------------------------------------
// Template test invokation.

INSTANTIATE_TYPED_TEST_CASE_P(
    DaoRdbUserAuthentication,
    DaoUserAuthentication,
    rdb::UserAuthentication);

INSTANTIATE_TYPED_TEST_CASE_P(
    DaoMemoryUserAuthentication,
    DaoUserAuthentication,
    memory::UserAuthentication);

} // namespace test
} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
