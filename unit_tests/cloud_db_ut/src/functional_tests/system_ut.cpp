/**********************************************************
* Oct 12, 2015
* akolesnikov
***********************************************************/

#include "system_ut.h"

#include <gtest/gtest.h>

#include <nx/network/http/httpclient.h>
#include <nx/utils/log/log_message.h>
#include <nx/utils/random.h>
#include <nx/utils/time.h>
#include <nx/utils/test_support/utils.h>

#include "test_setup.h"

namespace nx {
namespace cdb {

namespace {

class System:
    public CdbFunctionalTest
{
public:
    virtual void SetUp() override
    {
        CdbFunctionalTest::SetUp();
        ASSERT_TRUE(startAndWaitUntilStarted());
    }

protected:
    api::SystemData givenSystem()
    {
        auto owner = addActivatedAccount2();
        m_registeredAccounts.emplace(owner.email, owner);
        auto system = addRandomSystemToAccount(owner);
        return system;
    }

    AccountWithPassword givenUserOfSystem(const api::SystemData& system)
    {
        const auto newAccount = addActivatedAccount2();
        const auto owner = m_registeredAccounts.find(system.ownerAccountEmail)->second;
        shareSystemEx(owner, system, newAccount, api::SystemAccessRole::cloudAdmin);
        return newAccount;
    }

    void havingDisabledUserInSystem(
        const api::SystemData& system,
        const api::AccountData& user)
    {
        updateSharing(system, user, makeField(&api::SystemSharing::isEnabled, false));
    }

    void havingEnabledUserInSystem(
        const api::SystemData& system,
        const api::AccountData& user)
    {
        updateSharing(system, user, makeField(&api::SystemSharing::isEnabled, true));
    }

    void assertIfUserCanSeeSystem(
        const AccountWithPassword& user,
        const api::SystemData& systemToCheck)
    {
        ASSERT_FALSE(isUserHasAccessToSystem(user, systemToCheck));
    }

    void assertIfUserCannotSeeSystem(
        const AccountWithPassword& user,
        const api::SystemData& systemToCheck)
    {
        ASSERT_TRUE(isUserHasAccessToSystem(user, systemToCheck));
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

}

TEST_F(System, unbind)
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

    //adding system2 to account1
    api::SystemData system0;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, "vms opaque data", &system0));

    for (int i = 0; i < 4; ++i)
    {
        //adding system1 to account1
        api::SystemData system1;
        ASSERT_EQ(
            api::ResultCode::ok,
            bindRandomSystem(account1.email, account1Password, &system1));

        //checking account1 system list
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

        //sharing system1 with account2 as viewer
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
                //unbinding with owner credentials
                ASSERT_EQ(
                    api::ResultCode::ok,
                    unbindSystem(account1.email, account1Password, system1.id));
                break;
            case 1:
                //unbinding with system credentials
                ASSERT_EQ(
                    api::ResultCode::ok,
                    unbindSystem(system1.id, system1.authKey, system1.id));
                break;
            case 2:
                //unbinding with owner credentials
                ASSERT_EQ(
                    api::ResultCode::forbidden,
                    unbindSystem(account2.email, account2Password, system1.id));
                //unbinding with system credentials
                ASSERT_EQ(
                    api::ResultCode::ok,
                    unbindSystem(system1.id, system1.authKey, system1.id));
                continue;
            case 3:
                //unbinding with other system credentials
                ASSERT_EQ(
                    api::ResultCode::forbidden,
                    unbindSystem(system0.id, system0.authKey, system1.id));
                //unbinding with system credentials
                ASSERT_EQ(
                    api::ResultCode::ok,
                    unbindSystem(system1.id, system1.authKey, system1.id));
                continue;
        }

        //checking account1 system list
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

    //adding system2 to account1
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

    //requesting system1 using account2 credentials
    {
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::forbidden,
            testSetup->getSystem(account2.email, account2Password, system1.id, &systems));
    }

    {
        //requesting unknown system
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

TEST_F(System, get)
{
    cdbFunctionalTestSystemGet(this);
}

TEST_F(System, activation)
{
    api::AccountData account1;
    std::string account1Password;
    ASSERT_EQ(
        api::ResultCode::ok,
        addActivatedAccount(&account1, &account1Password));

    for (int i = 0; i < 1; ++i)
    {
        //adding system1 to account1
        api::SystemData system1;
        ASSERT_EQ(
            api::ResultCode::ok,
            bindRandomNotActivatedSystem(account1.email, account1Password, &system1));

        //checking account1 system list
        {
            std::vector<api::SystemDataEx> systems;
            ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
            ASSERT_EQ(systems.size(), 0U);   //only activated systems are provided
            //ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
            //ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
            //ASSERT_EQ(api::SystemStatus::ssNotActivated, systems[0].status);
        }

        if (i == 0)
        {
            api::NonceData nonceData;
            auto resultCode = getCdbNonce(
                system1.id,
                system1.authKey,
                &nonceData);
            ASSERT_EQ(api::ResultCode::ok, resultCode);
        }
        //else if (i == 1)
        //{
        //    //activating with ping
        //    api::NonceData nonceData;
        //    auto resultCode = ping(
        //        system1.id,
        //        system1.authKey);
        //    ASSERT_EQ(api::ResultCode::ok, resultCode);
        //}
        system1.status = api::SystemStatus::ssActivated;

        //checking account1 system list
        {
            std::vector<api::SystemDataEx> systems;
            ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
            ASSERT_EQ(systems.size(), 1U);
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
            ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
            ASSERT_EQ(api::SystemStatus::ssActivated, systems[0].status);
        }

        ASSERT_TRUE(restart());

        //checking account1 system list
        {
            std::vector<api::SystemDataEx> systems;
            ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
            ASSERT_EQ(systems.size(), 1U);
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
            ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
            ASSERT_EQ(api::SystemStatus::ssActivated, systems[0].status);
        }

        ASSERT_EQ(
            api::ResultCode::ok,
            unbindSystem(account1.email, account1Password, system1.id));
    }
}

constexpr static auto kSystemGoneForeverPeriod = std::chrono::seconds(5);
constexpr static auto kDropExpiredSystemsPeriodSec = std::chrono::seconds(1);

class SystemNotification:
    public System
{
public:
    SystemNotification()
    {
        addArg("-systemManager/reportRemovedSystemPeriod");
        addArg(QByteArray::number((unsigned int)kSystemGoneForeverPeriod.count()).constData());
        addArg("-systemManager/controlSystemStatusByDb");
        addArg("true");
        addArg("-systemManager/dropExpiredSystemsPeriod");
        addArg(QByteArray::number((unsigned int)kDropExpiredSystemsPeriodSec.count()).constData());
    }
};

TEST_F(SystemNotification, notification_of_system_removal)
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

        //adding system1 to account1
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

            //checking HTTP status code
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
                    ASSERT_TRUE(restart());
                continue;
            }
            else if (i == 1)
            {
                //waiting for result code to switch to notAuthorized
                std::this_thread::sleep_for(
                    kSystemGoneForeverPeriod + kDropExpiredSystemsPeriodSec*2);
                continue;
            }
        }
    }
}

TEST_F(System, rename)
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
            ASSERT_TRUE(restart());

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

TEST_F(System, persistent_sequence)
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
 * Validates order of elements in \a systems against \a systemIdsInSortOrder.
 * @param systemIdsInSortOrder Sorted by descending priority
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
            ASSERT_GT(prevSortingOrder, systems[i].usageFrequency);
        prevSortingOrder = systems[i].usageFrequency;
        ++i;
    }
}

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

TEST_F(System, sorting_order_weight_expiration)
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

    // A week has passed. No access.
    timeShift.applyRelativeShift(7 * std::chrono::hours(24));

    systems.clear();
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(account.email, account.password, &systems));
    const auto usageFrequency3 = systems[0].usageFrequency;

    ASSERT_LT(usageFrequency3, usageFrequency2);

    // Half year passed. Still no access.
    timeShift.applyRelativeShift(6 * 30 * std::chrono::hours(24));

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

TEST_F(System, sorting_order_multiple_systems)
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
            timeShift.applyAbsoluteShift(21 * std::chrono::hours(24));

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

TEST_F(System, sorting_order_last_login_time)
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

TEST_F(System, sorting_order_new_system_is_on_top)
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
            timeShift.applyAbsoluteShift(21 * std::chrono::hours(24));
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

TEST_F(System, sorting_order_persistence_after_sharing_update)
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
            ASSERT_TRUE(restart());

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

TEST_F(System, sorting_order_unknown_system)
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

TEST_F(System, update)
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
            ASSERT_TRUE(restart());

        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystems(account1.email, account1.password, &systems));
        ASSERT_EQ(1U, systems.size());
        ASSERT_EQ(system1, systems[0]);
        ASSERT_EQ(system1.opaque, systems[0].opaque);
    }
}

TEST_F(System, disabled_user_does_not_see_system)
{
    const auto system = givenSystem();
    const auto user = givenUserOfSystem(system);
    havingDisabledUserInSystem(system, user);
    assertIfUserCanSeeSystem(user, system);
}

TEST_F(System, reenabled_user_can_see_system)
{
    const auto system = givenSystem();
    const auto user = givenUserOfSystem(system);
    havingDisabledUserInSystem(system, user);
    havingEnabledUserInSystem(system, user);
    assertIfUserCannotSeeSystem(user, system);
}

} // namespace cdb
} // namespace nx
