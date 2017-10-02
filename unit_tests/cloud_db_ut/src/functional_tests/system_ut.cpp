#include "system_ut.h"

#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/utils/log/log_message.h>
#include <nx/utils/random.h>
#include <nx/utils/system_utils.h>
#include <nx/utils/string.h>
#include <nx/utils/time.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/cdb/dao/rdb/system_data_object.h>

#include "test_setup.h"

namespace nx {
namespace cdb {

namespace {

class FtSystem:
    public CdbFunctionalTest
{
public:
    virtual void SetUp() override
    {
        CdbFunctionalTest::SetUp();
        ASSERT_TRUE(startAndWaitUntilStarted());
    
        auto owner = addActivatedAccount2();
        m_registeredAccounts.emplace(owner.email, owner);
    }

    AccountWithPassword owner()
    {
        return m_registeredAccounts.begin()->second;
    }

protected:
    api::SystemData givenSystem()
    {
        return addRandomSystemToAccount(owner());
    }

    AccountWithPassword givenUserOfSystem(const api::SystemData& system)
    {
        const auto newAccount = addActivatedAccount2();
        const auto owner = m_registeredAccounts.find(system.ownerAccountEmail)->second;
        shareSystemEx(owner, system, newAccount, api::SystemAccessRole::cloudAdmin);
        return newAccount;
    }

    void whenUserIsDisabledInSystem(
        const api::SystemData& system,
        const api::AccountData& user)
    {
        updateSharing(system, user, makeField(&api::SystemSharing::isEnabled, false));
    }

    void whenUserIsEnabledInSystem(
        const api::SystemData& system,
        const api::AccountData& user)
    {
        updateSharing(system, user, makeField(&api::SystemSharing::isEnabled, true));
    }

    void whenCdbIsRestarted()
    {
        ASSERT_TRUE(restart());
    }

    void assertUserCannotSeeSystem(
        const AccountWithPassword& user,
        const api::SystemData& systemToCheck)
    {
        ASSERT_FALSE(isUserHasAccessToSystem(user, systemToCheck));
    }

    void assertUserCanSeeSystem(
        const AccountWithPassword& user,
        const api::SystemData& systemToCheck)
    {
        ASSERT_TRUE(isUserHasAccessToSystem(user, systemToCheck));
    }

    api::SystemData fetchSystem(std::string systemId)
    {
        const auto account = m_registeredAccounts.begin()->second;
        
        api::SystemDataEx system;
        auto resultCode = getSystem(
            account.email,
            account.password,
            systemId,
            &system);
        NX_GTEST_ASSERT_EQ(api::ResultCode::ok, resultCode);
        return system;
    }

private:
    std::map<std::string, AccountWithPassword> m_registeredAccounts;

    bool isUserHasAccessToSystem(
        const AccountWithPassword& user,
        const api::SystemData& systemToCheck)
    {
        std::vector<api::SystemDataEx> systems;
        NX_GTEST_ASSERT_EQ(api::ResultCode::ok, getSystems(user.email, user.password, &systems));
        const auto systemToCheckIter = std::find_if(
            systems.begin(),
            systems.end(),
            [&systemToCheck](const api::SystemDataEx& system)
            {
                return system.id == systemToCheck.id;
            });
        return systemToCheckIter != systems.end();
    }

    template<typename Name, typename Value>
    struct Field
    {
        Name name;
        Value value; 
    };

    template<typename Name, typename Value>
    Field<Name, Value> makeField(Name name, Value value)
    {
        Field<Name, Value> f;
        f.name = name;
        f.value = value;
        return f;
    }

    template<typename FieldType>
    void updateSharing(
        const api::SystemData& system,
        const api::AccountData& user,
        FieldType field)
    {
        api::SystemSharing sharing;
        sharing.accountEmail = user.email;
        sharing.accessRole = api::SystemAccessRole::cloudAdmin;
        sharing.isEnabled = true;
        sharing.systemId = system.id;
        const auto owner = m_registeredAccounts.find(system.ownerAccountEmail)->second;

        sharing.*field.name = field.value;

        ASSERT_EQ(api::ResultCode::ok, shareSystem(owner.email, owner.password, sharing));
    }
};

} // namespace

TEST_F(FtSystem, unbind)
{
    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account2, &account2Password));

    // Adding system2 to account1.
    api::SystemData system0;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, "vms opaque data", &system0));

    for (int i = 0; i < 4; ++i)
    {
        // Adding system1 to account1.
        api::SystemData system1;
        ASSERT_EQ(
            api::ResultCode::ok,
            bindRandomSystem(account1.email, account1Password, &system1));

        // Checking account1 system list.
        {
            std::vector<api::SystemDataEx> systems;
            ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
            ASSERT_EQ(systems.size(), 2U);
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system0) != systems.end());
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
            ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
            ASSERT_EQ(account1.fullName, systems[0].ownerFullName);
            ASSERT_EQ(account1.email, systems[1].ownerAccountEmail);
            ASSERT_EQ(account1.fullName, systems[1].ownerFullName);
        }

        // Sharing system1 with account2 as viewer.
        ASSERT_EQ(
            api::ResultCode::ok,
            shareSystem(
                account1.email,
                account1Password,
                system1.id,
                account2.email,
                api::SystemAccessRole::cloudAdmin));

        switch (i)
        {
            case 0:
                // Unbinding with owner credentials.
                ASSERT_EQ(
                    api::ResultCode::ok,
                    unbindSystem(account1.email, account1Password, system1.id));
                break;
            case 1:
                // Unbinding with system credentials.
                ASSERT_EQ(
                    api::ResultCode::ok,
                    unbindSystem(system1.id, system1.authKey, system1.id));
                break;
            case 2:
                // Unbinding with owner credentials.
                ASSERT_EQ(
                    api::ResultCode::forbidden,
                    unbindSystem(account2.email, account2Password, system1.id));
                // Unbinding with system credentials.
                ASSERT_EQ(
                    api::ResultCode::ok,
                    unbindSystem(system1.id, system1.authKey, system1.id));
                continue;
            case 3:
                // Unbinding with other system credentials.
                ASSERT_EQ(
                    api::ResultCode::forbidden,
                    unbindSystem(system0.id, system0.authKey, system1.id));
                // Unbinding with system credentials.
                ASSERT_EQ(
                    api::ResultCode::ok,
                    unbindSystem(system1.id, system1.authKey, system1.id));
                continue;
        }

        // Checking account1 system list.
        {
            std::vector<api::SystemDataEx> systems;
            ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
            ASSERT_EQ(systems.size(), 1U);
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system0) != systems.end());
            ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
        }

        ASSERT_TRUE(restart());
    }
}

void cdbFunctionalTestSystemGet(CdbFunctionalTest* testSetup)
{
    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        testSetup->addActivatedAccount(&account1, &account1Password));

    api::AccountData account2;
    std::string account2Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        testSetup->addActivatedAccount(&account2, &account2Password));

    // Adding system2 to account1.
    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        testSetup->bindRandomSystem(account1.email, account1Password, &system1));

    {
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::ok,
            testSetup->getSystem(account1.email, account1Password, system1.id, &systems));
        ASSERT_EQ(1U, systems.size());
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
        ASSERT_EQ(account1.fullName, systems[0].ownerFullName);
    }

    // Requesting system1 using account2 credentials.
    {
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::forbidden,
            testSetup->getSystem(account2.email, account2Password, system1.id, &systems));
    }

    {
        // Requesting unknown system.
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::forbidden,
            testSetup->getSystem(account1.email, account1Password, "unknown_system_id", &systems));
        ASSERT_TRUE(systems.empty());
    }

    {
        nx_http::HttpClient client;
        QUrl url(lit("http://127.0.0.1:%1/cdb/system/get?systemId=1").arg(testSetup->endpoint().port));
        url.setUserName(QString::fromStdString(account1.email));
        url.setPassword(QString::fromStdString(account1Password));
        ASSERT_TRUE(client.doGet(url));
    }

    {
        nx_http::HttpClient client;
        QUrl url(lit("http://127.0.0.1:%1/cdb/system/get?systemId=%2").
            arg(testSetup->endpoint().port).arg(QString::fromStdString(system1.id)));
        url.setUserName(QString::fromStdString(account1.email));
        url.setPassword(QString::fromStdString(account1Password));
        ASSERT_TRUE(client.doGet(url));
        const auto msgBody = client.fetchMessageBodyBuffer();
        ASSERT_NE(-1, msgBody.indexOf(QByteArray(system1.id.c_str())));
    }

    {
        nx_http::HttpClient client;
        QString urlStr(
            lm("http://127.0.0.1:%1/cdb/system/get?systemId=%2").
                arg(testSetup->endpoint().port).arg(QUrl::toPercentEncoding(QString::fromStdString(system1.id))));
        urlStr.replace(lit("{"), lit("%7B"));
        urlStr.replace(lit("}"), lit("%7D"));
        QUrl url(urlStr);
        url.setUserName(QString::fromStdString(account1.email));
        url.setPassword(QString::fromStdString(account1Password));
        ASSERT_TRUE(client.doGet(url));
        const auto msgBody = client.fetchMessageBodyBuffer();
        ASSERT_NE(-1, msgBody.indexOf(QByteArray(system1.id.c_str())));
    }
}

TEST_F(FtSystem, get)
{
    cdbFunctionalTestSystemGet(this);
}

//-------------------------------------------------------------------------------------------------
// FtSystemGetFilter

class FtSystemGetFilter:
    public FtSystem
{
protected:
    void givenAccountWithSystemsOfDifferentCustomizations()
    {
        m_account = addActivatedAccount2();

        constexpr int numberOfCustomizations = 3;
        for (int i = 0; i < numberOfCustomizations; ++i)
            m_customizations.push_back(nx::utils::generateRandomName(7).toStdString());

        constexpr int numberOfSystemsToAdd = 7;
        for (int i = 0; i < numberOfSystemsToAdd; ++i)
        {
            api::SystemData system;
            system.customization = m_customizations[
                nx::utils::random::number<std::size_t>(0, m_customizations.size() - 1)];
            m_systemByCustomization.emplace(
                system.customization,
                addRandomSystemToAccount(m_account, system));
        }
    }

    void whenRequestedSystemListWithCustomizationSpecified()
    {
        m_requestedCustomization = m_customizations[
            nx::utils::random::number<std::size_t>(0, m_customizations.size() - 1)];

        api::Filter filter;
        filter.nameToValue.emplace(api::FilterField::customization, m_requestedCustomization);

        ASSERT_EQ(
            api::ResultCode::ok,
            getSystemsFiltered(m_account.email, m_account.password, filter, &m_systemsReturned));
    }

    void thenSystemsOfRequestedCustomizationHaveBeenReturned()
    {
        const auto expectedSystemsRange = 
            m_systemByCustomization.equal_range(m_requestedCustomization);
        for (auto expectedSystemIter = expectedSystemsRange.first;
            expectedSystemIter != expectedSystemsRange.second;
            ++expectedSystemIter)
        {
            const auto receivedSystemIter =
                std::find_if(m_systemsReturned.begin(), m_systemsReturned.end(),
                    [&expectedSystemIter](const api::SystemDataEx& val)
                    {
                        return val.id == expectedSystemIter->second.id;
                    });
            ASSERT_TRUE(receivedSystemIter != m_systemsReturned.end());

            ASSERT_EQ(m_requestedCustomization, receivedSystemIter->customization);

            m_systemsReturned.erase(receivedSystemIter);
        }

        ASSERT_TRUE(m_systemsReturned.empty());
    }

private:
    AccountWithPassword m_account;
    std::multimap<std::string, api::SystemData> m_systemByCustomization;
    std::vector<std::string> m_customizations;
    std::string m_requestedCustomization;
    std::vector<api::SystemDataEx> m_systemsReturned;
};

TEST_F(FtSystemGetFilter, filter_by_customization)
{
    givenAccountWithSystemsOfDifferentCustomizations();
    whenRequestedSystemListWithCustomizationSpecified();
    thenSystemsOfRequestedCustomizationHaveBeenReturned();
}

//-------------------------------------------------------------------------------------------------
// FtSystemActivation

class TestSystemDataObject:
    public dao::rdb::SystemDataObject
{
    using base_type = dao::rdb::SystemDataObject;

public:
    TestSystemDataObject(const conf::Settings& settings):
        base_type(settings),
        m_failEveryActivateSystemRequest(false)
    {
    }

    virtual nx::utils::db::DBResult activateSystem(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId) override
    {
        nx::utils::db::DBResult result = nx::utils::db::DBResult::ok;
        if (m_failEveryActivateSystemRequest)
            result = nx::utils::db::DBResult::ioError;
        else
            result = base_type::activateSystem(queryContext, systemId);
        if (m_onActivateSystemDone)
            m_onActivateSystemDone(result);
        return result;
    }

    void failEveryActivateSystemRequest()
    {
        m_failEveryActivateSystemRequest = true;
    }

    void switchToNormalState()
    {
        m_failEveryActivateSystemRequest = false;
    }

    void setOnActivateSystemDone(nx::utils::MoveOnlyFunc<void(nx::utils::db::DBResult)> handler)
    {
        m_onActivateSystemDone = std::move(handler);
    }

private:
    bool m_failEveryActivateSystemRequest;
    nx::utils::MoveOnlyFunc<void(nx::utils::db::DBResult)> m_onActivateSystemDone;
};

class FtSystemActivation:
    public FtSystem
{
public:
    FtSystemActivation():
        m_systemDao(nullptr)
    {
        using namespace std::placeholders;

        m_systemDaoFactoryBak = dao::SystemDataObjectFactory::setCustomFactoryFunc(
            std::bind(&FtSystemActivation::createSystemDao, this, _1));
    }

    ~FtSystemActivation()
    {
        dao::SystemDataObjectFactory::setCustomFactoryFunc(std::move(m_systemDaoFactoryBak));
    }

protected:
    void givenNotActivatedSystem()
    {
        ASSERT_EQ(
            api::ResultCode::ok,
            bindRandomNotActivatedSystem(owner().email, owner().password, &m_system));
    }

    void givenActivatedSystem()
    {
        givenNotActivatedSystem();
        whenIssuedRequestUsingSystemCredentials();
    }

    void whenIssuedRequestUsingSystemCredentials()
    {
        api::NonceData nonceData;
        auto resultCode = getCdbNonce(
            m_system.id,
            m_system.authKey,
            &nonceData);
        ASSERT_EQ(api::ResultCode::ok, resultCode);
    }

    void assertGetSystemsReturnedEmptyList()
    {
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystems(owner().email, owner().password, &systems));
        ASSERT_TRUE(systems.empty());
    }

    void assertActivatedSystemPresentInGetSystemsResponse()
    {
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystems(owner().email, owner().password, &systems));
        ASSERT_EQ(1U, systems.size());

        m_system.status = api::SystemStatus::activated;
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), m_system) != systems.end());
        ASSERT_EQ(api::SystemStatus::activated, systems[0].status);

        ASSERT_EQ(owner().email, systems[0].ownerAccountEmail);
    }

    void assertDaoHasFailedActivateSystemRequest()
    {
        ASSERT_NE(nx::utils::db::DBResult::ok, m_activateSystemResults.pop());
    }

    void assertSystemActivationHasBeenSavedToDb()
    {
        ASSERT_EQ(nx::utils::db::DBResult::ok, m_activateSystemResults.pop());
    }

    TestSystemDataObject& systemDao()
    {
        return *m_systemDao;
    }

private:
    api::SystemData m_system;
    dao::SystemDataObjectFactory::CustomFactoryFunc m_systemDaoFactoryBak;
    TestSystemDataObject* m_systemDao;
    nx::utils::SyncQueue<nx::utils::db::DBResult> m_activateSystemResults;

    std::unique_ptr<dao::AbstractSystemDataObject> createSystemDao(
        const conf::Settings& settings)
    {
        using namespace std::placeholders;

        auto systemDao = std::make_unique<TestSystemDataObject>(settings);
        m_systemDao = systemDao.get();
        m_systemDao->setOnActivateSystemDone(
            std::bind(&FtSystemActivation::onActivateSystemDone, this, _1));
        return std::move(systemDao);
    }

    void onActivateSystemDone(nx::utils::db::DBResult dbResult)
    {
        m_activateSystemResults.push(dbResult);
    }
};

TEST_F(FtSystemActivation, not_activated_system_does_not_present_in_system_list)
{
    givenNotActivatedSystem();
    assertGetSystemsReturnedEmptyList();
}

TEST_F(
    FtSystemActivation,
    system_becomes_activated_after_issuing_request_authenticated_with_system_credentials)
{
    givenNotActivatedSystem();
    whenIssuedRequestUsingSystemCredentials();
    assertActivatedSystemPresentInGetSystemsResponse();
}

TEST_F(FtSystemActivation, system_activation_is_persistent)
{
    givenActivatedSystem();
    whenCdbIsRestarted();
    assertActivatedSystemPresentInGetSystemsResponse();
}

TEST_F(FtSystemActivation, system_activation_is_saved_to_db_after_db_error)
{
    systemDao().failEveryActivateSystemRequest();
    
    givenNotActivatedSystem();
    whenIssuedRequestUsingSystemCredentials();
    assertDaoHasFailedActivateSystemRequest();

    systemDao().switchToNormalState();
    whenIssuedRequestUsingSystemCredentials();
    assertSystemActivationHasBeenSavedToDb();

    whenCdbIsRestarted();
    assertActivatedSystemPresentInGetSystemsResponse();
}

//-------------------------------------------------------------------------------------------------
// FtSystemNotification

constexpr static auto kSystemGoneForeverPeriod = std::chrono::seconds(5);
constexpr static auto kDropExpiredSystemsPeriodSec = std::chrono::seconds(1);

class FtSystemNotification:
    public FtSystem
{
public:
    FtSystemNotification()
    {
        addArg("-systemManager/reportRemovedSystemPeriod");
        addArg(QByteArray::number((unsigned int)kSystemGoneForeverPeriod.count()).constData());
        addArg("-systemManager/controlSystemStatusByDb");
        addArg("true");
        addArg("-systemManager/dropExpiredSystemsPeriod");
        addArg(QByteArray::number((unsigned int)kDropExpiredSystemsPeriodSec.count()).constData());
    }
};

TEST_F(FtSystemNotification, notification_of_system_removal)
{
    enum class TestOption
    {
        withRestart,
        withoutRestart
    };

    const TestOption testOptions[] = { TestOption::withRestart, TestOption::withoutRestart };

    for (const auto& testOption: testOptions)
    {
        api::AccountData account1;
        std::string account1Password;
        ASSERT_EQ(
            api::ResultCode::ok,
            addActivatedAccount(&account1, &account1Password));

        // Adding system1 to account1.
        api::SystemData system1;
        ASSERT_EQ(
            api::ResultCode::ok,
            bindRandomSystem(account1.email, account1Password, &system1));

        api::NonceData nonceData;
        ASSERT_EQ(
            api::ResultCode::ok,
            getCdbNonce(system1.id, system1.authKey, &nonceData));

        const auto timeJustBeforeSystemRemoval = std::chrono::steady_clock::now();
        ASSERT_EQ(
            api::ResultCode::ok,
            unbindSystem(account1.email, account1Password, system1.id));

        for (int i = 0; i < 3; ++i)
        {
            api::NonceData nonceData;
            const auto timePassedSinceSystemRemoval = 
                std::chrono::steady_clock::now() - timeJustBeforeSystemRemoval;
            ASSERT_EQ(
                timePassedSinceSystemRemoval < kSystemGoneForeverPeriod
                    ? api::ResultCode::credentialsRemovedPermanently
                    : api::ResultCode::notAuthorized,
                getCdbNonce(system1.id, system1.authKey, &nonceData));

            // Checking HTTP status code.
            nx_http::HttpClient httpClient;
            QUrl requestUrl(lit("http://127.0.0.1:%1/cdb/auth/get_nonce").arg(endpoint().port));
            requestUrl.setUserName(QString::fromStdString(system1.id));
            requestUrl.setPassword(QString::fromStdString(system1.authKey));
            ASSERT_TRUE(httpClient.doGet(requestUrl));
            ASSERT_NE(nullptr, httpClient.response());
            ASSERT_EQ(
                nx_http::StatusCode::unauthorized,
                httpClient.response()->statusLine.statusCode);

            if (i == 0)
            {
                if (testOption == TestOption::withRestart)
                {
                    ASSERT_TRUE(restart());
                }
                continue;
            }
            else if (i == 1)
            {
                // Waiting for result code to switch to notAuthorized.
                std::this_thread::sleep_for(
                    kSystemGoneForeverPeriod + kDropExpiredSystemsPeriodSec*2);
                continue;
            }
        }
    }
}

TEST_F(FtSystem, rename)
{
    const auto account1 = addActivatedAccount2();
    // Adding system1 to account1.
    const auto system1 = addRandomSystemToAccount(account1);

    const auto account2 = addActivatedAccount2();
    shareSystemEx(account1, system1, account2, api::SystemAccessRole::cloudAdmin);

    const auto account3 = addActivatedAccount2();
    shareSystemEx(account1, system1, account3, api::SystemAccessRole::localAdmin);

    const auto account4 = addActivatedAccount2();
    shareSystemEx(account1, system1, account4, api::SystemAccessRole::advancedViewer);

    std::string actualSystemName = "new system name";
    // Owner is allowed to rename his system.
    ASSERT_EQ(
        api::ResultCode::ok,
        renameSystem(
            account1.email, account1.password,
            system1.id, actualSystemName));

    for (int j = 0; j < 2; ++j)
    {
        const bool restartRequired = j == 1;

        if (restartRequired)
        {
            ASSERT_TRUE(restart());
        }

        // Checking system1 name.
        api::SystemDataEx systemData;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystem(account1.email, account1.password, system1.id, &systemData));
        ASSERT_EQ(actualSystemName, systemData.name);
    }

    // Owner and admin can rename system.
    actualSystemName = "sdfn[rtsdh";
    ASSERT_EQ(
        api::ResultCode::ok,
        renameSystem(account2.email, account2.password, system1.id, actualSystemName));

    actualSystemName = "sdfn[rtsdh111";
    ASSERT_EQ(
        api::ResultCode::ok,
        renameSystem(account3.email, account3.password, system1.id, actualSystemName));

    ASSERT_EQ(
        api::ResultCode::forbidden,
        renameSystem(account4.email, account4.password, system1.id, "xxx"));

    ASSERT_EQ(
        api::ResultCode::forbidden,
        renameSystem(system1.id, system1.authKey, system1.id, "aaa"));

    // Trying bad system names.
    ASSERT_EQ(
        api::ResultCode::badRequest,
        renameSystem(
            account1.email, account1.password,
            system1.id, std::string()));
    ASSERT_EQ(
        api::ResultCode::badRequest,
        renameSystem(
            account1.email, account1.password,
            system1.id, std::string(4096, 'z')));

    // Checking system1 name.
    api::SystemDataEx systemData;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystem(account1.email, account1.password, system1.id, &systemData));
    ASSERT_EQ(actualSystemName, systemData.name);
}

TEST_F(FtSystem, persistent_sequence)
{
    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system1));

    api::SystemData system2;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system2));

    ASSERT_EQ(system1.systemSequence+1, system2.systemSequence);

    ASSERT_TRUE(restart());

    api::SystemDataEx systemData;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystem(account1.email, account1Password, system1.id, &systemData));
    ASSERT_EQ(system1.systemSequence, systemData.systemSequence);

    ASSERT_EQ(
        api::ResultCode::ok,
        getSystem(account1.email, account1Password, system2.id, &systemData));
    ASSERT_EQ(system2.systemSequence, systemData.systemSequence);

    api::SystemData system3;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system3));
    ASSERT_EQ(system2.systemSequence + 1, system3.systemSequence);
}

/**
 * Validates order of elements in systems against systemIdsInSortOrder.
 * systemIdsInSortOrder Sorted by descending priority
 */
static void validateSystemsOrder(
    const std::list<std::string>& systemIdsInSortOrder,
    const std::vector<api::SystemDataEx>& systems)
{
    ASSERT_EQ(systemIdsInSortOrder.size(), systems.size());

    std::size_t i = 0;
    boost::optional<float> prevSortingOrder;
    for (const auto& systemId: systemIdsInSortOrder)
    {
        ASSERT_EQ(systemId, systems[i].id);
        if (prevSortingOrder)
        {
            ASSERT_GT(prevSortingOrder, systems[i].usageFrequency);
        }
        prevSortingOrder = systems[i].usageFrequency;
        ++i;
    }
}

class FtSystemSortingOrder:
    public FtSystem
{
protected:
    template<typename Container>
    void bringToTop(
        Container& container,
        typename Container::value_type value)
    {
        const auto it = std::find(container.begin(), container.end(), value);
        if (it != container.end())
            container.erase(it);
        container.push_front(std::move(value));
    }
};

TEST_F(FtSystemSortingOrder, weight_expiration)
{
    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::system);

    const auto account = addActivatedAccount2();
    const auto system1 = addRandomSystemToAccount(account);

    // First access.
    ASSERT_EQ(
        api::ResultCode::ok,
        recordUserSessionStart(account, system1.id));

    std::vector<api::SystemDataEx> systems;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(account.email, account.password, &systems));
    const auto usageFrequency1 = systems[0].usageFrequency;

    // Second access.
    ASSERT_EQ(
        api::ResultCode::ok,
        recordUserSessionStart(account, system1.id));

    systems.clear();
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(account.email, account.password, &systems));
    const auto usageFrequency2 = systems[0].usageFrequency;

    ASSERT_GT(usageFrequency2, usageFrequency1);

    timeShift.applyRelativeShift(
        static_cast<int>(nx::utils::kSystemAccessBurnPeriodFullDays * 0.25) *
        std::chrono::hours(24));

    systems.clear();
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(account.email, account.password, &systems));
    const auto usageFrequency3 = systems[0].usageFrequency;

    ASSERT_LT(usageFrequency3, usageFrequency2);

    timeShift.applyRelativeShift(
        static_cast<int>(nx::utils::kSystemAccessBurnPeriodFullDays * 6) *
        std::chrono::hours(24));

    systems.clear();
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(account.email, account.password, &systems));
    const auto usageFrequency4 = systems[0].usageFrequency;

    ASSERT_LT(usageFrequency4, usageFrequency3);

    // Repeating previous check but after restart.
    ASSERT_TRUE(restart());

    systems.clear();
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(account.email, account.password, &systems));
    const auto usageFrequency5 = systems[0].usageFrequency;

    ASSERT_EQ(usageFrequency4, usageFrequency5);
}

TEST_F(FtSystemSortingOrder, multiple_systems)
{
    const auto account = addActivatedAccount2();
    const auto system1 = addRandomSystemToAccount(account);
    const auto system2 = addRandomSystemToAccount(account);
    const auto system3 = addRandomSystemToAccount(account);

    for (int i = 0; i < 1; ++i)
        ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system1.id));
    for (int i = 0; i < 2; ++i)
        ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system2.id));
    for (int i = 0; i < 3; ++i)
        ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system3.id));

    // First element has highest priority.
    std::list<std::string> systemIdsInSortOrder;
    systemIdsInSortOrder.push_back(system3.id);
    systemIdsInSortOrder.push_back(system2.id);
    systemIdsInSortOrder.push_back(system1.id);

    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::system);

    for (int i = 0; i < 3; ++i)
    {
        if (i == 1)
        {
            // Shifting time and testing for access history expiration.
            timeShift.applyAbsoluteShift(
                static_cast<int>(nx::utils::kSystemAccessBurnPeriodFullDays * 0.66) * std::chrono::hours(24));

            for (int j = 0; j < 2; ++j)
                ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system1.id));
            bringToTop(systemIdsInSortOrder, system1.id);
        }
        else if (i == 2)
        {
            ASSERT_TRUE(restart());
        }

        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystems(account.email, account.password, &systems));

        ASSERT_EQ(3U, systems.size());
        std::sort(
            systems.begin(), systems.end(),
            [](const api::SystemDataEx& one, const api::SystemDataEx& two)
            {
                return one.usageFrequency > two.usageFrequency; //< Descending sort.
            });

        // Comparing with systemIdsInSortOrder.
        validateSystemsOrder(systemIdsInSortOrder, systems);
    }
}

TEST_F(FtSystemSortingOrder, last_login_time)
{
    const auto account = addActivatedAccount2();
    const auto system1 = addRandomSystemToAccount(account);

    std::vector<api::SystemDataEx> systems;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(account.email, account.password, &systems));
    const auto lastLoginTime1 = systems[0].lastLoginTime;
    // Initial value for lastLoginTime is current time.
    ASSERT_GT(systems[0].lastLoginTime, nx::utils::utcTime() - std::chrono::seconds(10));
    ASSERT_LT(systems[0].lastLoginTime, nx::utils::utcTime() + std::chrono::seconds(10));

    ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system1.id));

    systems.clear();
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(account.email, account.password, &systems));
    const auto lastLoginTime2 = systems[0].lastLoginTime;
    ASSERT_GT(lastLoginTime2, lastLoginTime1);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system1.id));

    systems.clear();
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(account.email, account.password, &systems));
    const auto lastLoginTime3 = systems[0].lastLoginTime;

    ASSERT_GT(lastLoginTime3, lastLoginTime2);
    ASSERT_GE(lastLoginTime3-lastLoginTime2, std::chrono::seconds(1));

    // Checking that lastLoginTime is close to real current time.
    ASSERT_GT(lastLoginTime3, nx::utils::utcTime() - std::chrono::seconds(10));
    ASSERT_LT(lastLoginTime3, nx::utils::utcTime() + std::chrono::seconds(10));
}

TEST_F(FtSystemSortingOrder, new_system_is_on_top)
{
    const auto account = addActivatedAccount2();
    const auto system1 = addRandomSystemToAccount(account);

    ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system1.id));
    std::vector<api::SystemDataEx> systems;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(account.email, account.password, &systems));

    const auto system2 = addRandomSystemToAccount(account);

    std::list<std::string> systemIdsInSortOrder;
    systemIdsInSortOrder.push_back(system2.id);
    systemIdsInSortOrder.push_back(system1.id);

    for (int i = 0; i < 4; ++i)
    {
        const bool checkingThatNewSystemStaysonStopAfterAccess = i == 1;
        const bool needRestart = i == 2;
        const bool bringNewSystemDown = i == 3;
        if (checkingThatNewSystemStaysonStopAfterAccess)
        {
            ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system1.id));
            ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system2.id));
        }
        if (needRestart)
        {
            ASSERT_TRUE(restart());
        }
        nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::system);
        if (bringNewSystemDown)
        {
            timeShift.applyAbsoluteShift(
                static_cast<int>(nx::utils::kSystemAccessBurnPeriodFullDays * 0.66) * std::chrono::hours(24));
            ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system1.id));

            bringToTop(systemIdsInSortOrder, system1.id);
        }

        systems.clear();
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystems(account.email, account.password, &systems));

        std::sort(
            systems.begin(), systems.end(),
            [](const api::SystemDataEx& one, const api::SystemDataEx& two)
            {
                return one.usageFrequency > two.usageFrequency; //< Descending sort.
            });

        validateSystemsOrder(systemIdsInSortOrder, systems);
    }
}

TEST_F(FtSystemSortingOrder, persistence_after_sharing_update)
{
    const auto account1 = addActivatedAccount2();
    const auto system1 = addRandomSystemToAccount(account1);
    const auto account2 = addActivatedAccount2();
    const auto system2 = addRandomSystemToAccount(account2);

    shareSystemEx(account1, system1, account2, api::SystemAccessRole::cloudAdmin);

    std::list<std::string> systemIdsInSortOrder;
    systemIdsInSortOrder.push_back(system1.id);
    systemIdsInSortOrder.push_back(system2.id);

    for (int i = 0; i < 3; ++i)
    {
        const bool updateSharing = i == 1;
        const bool needRestart = i == 2;
        if (updateSharing)
            shareSystemEx(account1, system1, account2, api::SystemAccessRole::advancedViewer);
        if (needRestart)
        {
            ASSERT_TRUE(restart());
        }

        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystems(account2.email, account2.password, &systems));
        std::sort(
            systems.begin(), systems.end(),
            [](const api::SystemDataEx& one, const api::SystemDataEx& two)
            {
                return one.usageFrequency > two.usageFrequency; //< Descending sort.
            });
        validateSystemsOrder(systemIdsInSortOrder, systems);
    }
}

TEST_F(FtSystemSortingOrder, unknown_system)
{
    const auto account1 = addActivatedAccount2();
    const auto system1 = addRandomSystemToAccount(account1);

    ASSERT_EQ(
        api::ResultCode::ok,
        recordUserSessionStart(account1, system1.id));
    ASSERT_EQ(
        api::ResultCode::notFound,
        recordUserSessionStart(account1, "{"+system1.id+"}"));
}

TEST_F(FtSystem, update)
{
    constexpr const char kOpaqueValue[] = 
        "SELECT * FROM account WHERE email like 'test@example.com'\r\n "
        "OR email like '\\slashed_test@example.com'\n";

    const auto account1 = addActivatedAccount2();
    auto system1 = addRandomSystemToAccount(account1);

    api::SystemAttributesUpdate updatedData;
    updatedData.systemId = system1.id;
    updatedData.opaque = kOpaqueValue;
    ASSERT_EQ(
        api::ResultCode::ok,
        updateSystem(system1, updatedData));

    system1.opaque = updatedData.opaque.get();

    for (int i = 0; i < 2; ++i)
    {
        if (i == 1)
        {
            ASSERT_TRUE(restart());
        }

        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystems(account1.email, account1.password, &systems));
        ASSERT_EQ(1U, systems.size());
        ASSERT_EQ(system1, systems[0]);
        ASSERT_EQ(system1.opaque, systems[0].opaque);
    }
}

TEST_F(FtSystem, disabled_user_does_not_see_system)
{
    const auto system = givenSystem();
    const auto user = givenUserOfSystem(system);
    whenUserIsDisabledInSystem(system, user);
    assertUserCannotSeeSystem(user, system);
}

TEST_F(FtSystem, reenabled_user_can_see_system)
{
    const auto system = givenSystem();
    const auto user = givenUserOfSystem(system);
    whenUserIsDisabledInSystem(system, user);
    whenUserIsEnabledInSystem(system, user);
    assertUserCanSeeSystem(user, system);
}

class FtSystemTimestamp:
    public FtSystem
{
protected:
    void whenSystemIsRegistered()
    {
        m_registrationTimeValidRange.first = nx::utils::utcTime();
        m_system = givenSystem();
        m_registrationTimeValidRange.second = nx::utils::utcTime();
    }
    
    void assertRegistrationTimestampIsValid()
    {
        using namespace std::chrono;

        const auto system = fetchSystem(m_system.id);
        ASSERT_GE(
            nx::utils::floor<milliseconds>(system.registrationTime),
            nx::utils::floor<milliseconds>(m_registrationTimeValidRange.first));
        ASSERT_LE(
            nx::utils::floor<milliseconds>(system.registrationTime),
            nx::utils::floor<milliseconds>(m_registrationTimeValidRange.second));
    }
    
private:
    std::pair<
        std::chrono::system_clock::time_point,
        std::chrono::system_clock::time_point
    > m_registrationTimeValidRange;
    api::SystemData m_system;
};

TEST_F(FtSystemTimestamp, registration_timestamp)
{
    whenSystemIsRegistered();
    assertRegistrationTimestampIsValid();

    whenCdbIsRestarted();

    assertRegistrationTimestampIsValid();
}

} // namespace cdb
} // namespace nx
