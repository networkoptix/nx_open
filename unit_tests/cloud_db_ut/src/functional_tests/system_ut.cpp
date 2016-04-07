/**********************************************************
* Oct 12, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <nx/network/http/httpclient.h>
#include <nx/utils/log/log_message.h>

#include "test_setup.h"


namespace nx {
namespace cdb {

TEST_F(CdbFunctionalTest, system_unbind)
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

TEST_F(CdbFunctionalTest, system_get)
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
    api::SystemData system1;
    ASSERT_EQ(
        api::ResultCode::ok,
        bindRandomSystem(account1.email, account1Password, &system1));

    {
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystem(account1.email, account1Password, system1.id, &systems));
        ASSERT_EQ(1, systems.size());
        ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
        ASSERT_EQ(account1.fullName, systems[0].ownerFullName);
    }

    //requesting system1 using account2 credentials
    {
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::forbidden,
            getSystem(account2.email, account2Password, system1.id, &systems));
    }

    {
        //requesting unknown system
        std::vector<api::SystemDataEx> systems;
        ASSERT_EQ(
            api::ResultCode::forbidden,
            getSystem(account1.email, account1Password, "unknown_system_id", &systems));
        ASSERT_TRUE(systems.empty());
    }

    {
        nx_http::HttpClient client;
        QUrl url(lit("http://127.0.0.1:%1/cdb/system/get?systemID=1").arg(endpoint().port));
        url.setUserName(QString::fromStdString(account1.email));
        url.setPassword(QString::fromStdString(account1Password));
        ASSERT_TRUE(client.doGet(url));
    }

    {
        nx_http::HttpClient client;
        QUrl url(lit("http://127.0.0.1:%1/cdb/system/get?systemID=%2").
            arg(endpoint().port).arg(QString::fromStdString(system1.id)));
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
                arg(endpoint().port).arg(QUrl::toPercentEncoding(QString::fromStdString(system1.id))));
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

TEST_F(CdbFunctionalTest, system_activation)
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

TEST_F(CdbFunctionalTest, notification_of_system_removal)
{
    constexpr const std::chrono::seconds kSystemGoneForeverPeriod = std::chrono::seconds(7);

    addArg("-systemManager/reportRemovedSystemPeriodSec");
    addArg(QByteArray::number((unsigned int)kSystemGoneForeverPeriod.count()).constData());

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

        ASSERT_EQ(
            api::ResultCode::ok,
            unbindSystem(account1.email, account1Password, system1.id));

        for (int i = 0; i < 2; ++i)
        {
            api::NonceData nonceData;
            ASSERT_EQ(
                api::ResultCode::credentialsRemovedPermanently,
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
                std::this_thread::sleep_for(kSystemGoneForeverPeriod);

                ASSERT_EQ(
                    api::ResultCode::notAuthorized,
                    getCdbNonce(system1.id, system1.authKey, &nonceData));
            }
        }
    }
}

}   //cdb
}   //nx
