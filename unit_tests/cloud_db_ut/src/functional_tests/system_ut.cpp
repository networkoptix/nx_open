/**********************************************************
* Oct 12, 2015
* akolesnikov
***********************************************************/

#include "system_ut.h"

#include <gtest/gtest.h>

#include <nx/network/http/httpclient.h>
#include <nx/utils/log/log_message.h>
#include <nx/utils/random.h>

#include "test_setup.h"


namespace nx {
namespace cdb {

namespace {
class System
:
    public CdbFunctionalTest
{
};
}

TEST_F(System, unbind)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

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
        bindRandomSystem(account1.email, account1Password, &system0));

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
            ASSERT_EQ(systems.size(), 2);
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
            ASSERT_EQ(systems.size(), 1);
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system0) != systems.end());
            ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
        }

        restart();
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
        ASSERT_EQ(1, systems.size());
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
        QUrl url(lit("http://127.0.0.1:%1/cdb/system/get?systemID=1").arg(testSetup->endpoint().port));
        url.setUserName(QString::fromStdString(account1.email));
        url.setPassword(QString::fromStdString(account1Password));
        ASSERT_TRUE(client.doGet(url));
    }

    {
        nx_http::HttpClient client;
        QUrl url(lit("http://127.0.0.1:%1/cdb/system/get?systemID=%2").
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
            lm("http://127.0.0.1:%1/cdb/system/get?systemID=%2").
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
    ASSERT_TRUE(startAndWaitUntilStarted());
    cdbFunctionalTestSystemGet(this);
}

TEST_F(System, activation)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

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
            ASSERT_EQ(systems.size(), 0);   //only activated systems are provided
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
            ASSERT_EQ(systems.size(), 1);
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
            ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
            ASSERT_EQ(api::SystemStatus::ssActivated, systems[0].status);
        }

        restart();

        //checking account1 system list
        {
            std::vector<api::SystemDataEx> systems;
            ASSERT_EQ(getSystems(account1.email, account1Password, &systems), api::ResultCode::ok);
            ASSERT_EQ(systems.size(), 1);
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
            ASSERT_EQ(account1.email, systems[0].ownerAccountEmail);
            ASSERT_EQ(api::SystemStatus::ssActivated, systems[0].status);
        }

        ASSERT_EQ(
            api::ResultCode::ok,
            unbindSystem(account1.email, account1Password, system1.id));
    }
}

TEST_F(System, notification_of_system_removal)
{
    constexpr const std::chrono::seconds kSystemGoneForeverPeriod =
        std::chrono::seconds(5);
    constexpr const std::chrono::seconds kDropExpiredSystemsPeriodSec =
        std::chrono::seconds(1);

    addArg("-systemManager/reportRemovedSystemPeriod");
    addArg(QByteArray::number((unsigned int)kSystemGoneForeverPeriod.count()).constData());
    addArg("-systemManager/controlSystemStatusByDb");
    addArg("true");
    addArg("-systemManager/dropExpiredSystemsPeriod");
    addArg(QByteArray::number((unsigned int)kDropExpiredSystemsPeriodSec.count()).constData());

    ASSERT_TRUE(startAndWaitUntilStarted());

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
                    restart();
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
    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto account1 = addActivatedAccount2();
    // Adding system1 to account1.
    const auto system1 = addRandomSystemToAccount(account1);

    const auto account2 = addActivatedAccount2();
    shareSystem2(account1, system1, account2, api::SystemAccessRole::cloudAdmin);

    const std::string actualSystemName = "new system name";
    // Owner is allowed to rename his system.
    ASSERT_EQ(
        api::ResultCode::ok,
        renameSystem(
            account1.data.email, account1.password,
            system1.id, actualSystemName));

    for (int j = 0; j < 2; ++j)
    {
        const bool restartRequired = j == 1;

        if (restartRequired)
            restart();

        // Checking system1 name.
        api::SystemDataEx systemData;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystem(account1.data.email, account1.password, system1.id, &systemData));
        ASSERT_EQ(actualSystemName, systemData.name);
    }

    // Only owner can rename system.
    ASSERT_EQ(
        api::ResultCode::forbidden,
        renameSystem(system1.id, system1.authKey, system1.id, "aaa"));

    ASSERT_EQ(
        api::ResultCode::forbidden,
        renameSystem(account2.data.email, account2.password, system1.id, "xxx"));

    // Trying bad system names.
    ASSERT_EQ(
        api::ResultCode::badRequest,
        renameSystem(
            account1.data.email, account1.password,
            system1.id, std::string()));
    ASSERT_EQ(
        api::ResultCode::badRequest,
        renameSystem(
            account1.data.email, account1.password,
            system1.id, std::string(4096, 'z')));

    // Checking system1 name.
    api::SystemDataEx systemData;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystem(account1.data.email, account1.password, system1.id, &systemData));
    ASSERT_EQ(actualSystemName, systemData.name);
}

TEST_F(System, persistentSequence)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

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

    restart();

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

TEST_F(System, sortingOrder)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    const auto account = addActivatedAccount2();
    const auto system1 = addRandomSystemToAccount(account);
    const auto system2 = addRandomSystemToAccount(account);
    const auto system3 = addRandomSystemToAccount(account);

    for (int i = 0; i < 3; ++i)
        ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system1.id));
    for (int i = 0; i < 2; ++i)
        ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system2.id));
    for (int i = 0; i < 1; ++i)
        ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system3.id));

    std::list<std::string> systemIdsInSortOrder;
    systemIdsInSortOrder.push_front(system1.id);
    systemIdsInSortOrder.push_front(system2.id);
    systemIdsInSortOrder.push_front(system3.id);

    //for (int i = 0; i < 2; ++i)
    {
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystems(account.data.email, account.password, &systems));

        ASSERT_EQ(3, systems.size());
        std::sort(
            systems.begin(), systems.end(),
            [](const api::SystemDataEx& one, const api::SystemDataEx& two)
            {
                return one.sortingOrder > two.sortingOrder; //< Descending sort.
            });

        // TODO: #ak: comparing with systemIdsInSortOrder

        ASSERT_GT(systems[0].sortingOrder, systems[1].sortingOrder);
        ASSERT_GT(systems[1].sortingOrder, systems[2].sortingOrder);

        ASSERT_EQ(system1.id, systems[0].id);
        ASSERT_EQ(system2.id, systems[1].id);
        ASSERT_EQ(system3.id, systems[2].id);

        // TODO: #ak: shifting time and testing for access history expiration

        ASSERT_EQ(api::ResultCode::ok, recordUserSessionStart(account, system3.id));
    }
}

}   //cdb
}   //nx
