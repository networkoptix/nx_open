/**********************************************************
* Nov 23, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <utils/common/model_functions.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/server/fusion_request_result.h>
#include <cloud_db_client/src/data/account_data.h>
#include <cloud_db_client/src/data/types.h>

#include "test_setup.h"
#include "version.h"


namespace nx {
namespace cdb {

TEST_F(CdbFunctionalTest, api_conventions_general)
{
    //waiting for cloud_db initialization
    startAndWaitUntilStarted();

    api::AccountData account1;
    std::string account1Password;
    api::AccountConfirmationCode activationCode;
    auto result = addAccount(&account1, &account1Password, &activationCode);
    ASSERT_EQ(result, api::ResultCode::ok);

    {
        //missing required parameter
        nx_http::HttpClient httpClient;
        QUrl url(lit("http://%1:%2/system/bind?name=esadfwer").arg(endpoint().address.toString()).arg(endpoint().port));
        url.setUserName(QString::fromStdString(account1.email));
        url.setPassword(QString::fromStdString(account1Password));
        ASSERT_TRUE(httpClient.doGet(url));

        auto msgBody = httpClient.fetchMessageBodyBuffer();
        nx_http::FusionRequestResult requestResult = 
            QJson::deserialized<nx_http::FusionRequestResult>(msgBody);

        ASSERT_TRUE(httpClient.response() != nullptr);
        ASSERT_EQ(nx_http::StatusCode::badRequest, httpClient.response()->statusLine.statusCode);
        ASSERT_EQ(nx_http::FusionRequestErrorClass::badRequest, requestResult.errorClass);
        ASSERT_EQ(
            QnLexical::serialized(nx_http::FusionRequestErrorDetail::deserializationError),
            requestResult.resultCode);
        ASSERT_EQ(
            (int)nx_http::FusionRequestErrorDetail::deserializationError,
            requestResult.errorDetail);
    }

    {
        //operation forbidden for account in this state
        nx_http::HttpClient httpClient;
        QUrl url(lit("http://%1:%2/system/bind?name=esadfwer&customization=%3").
            arg(endpoint().address.toString()).arg(endpoint().port).arg(QN_CUSTOMIZATION_NAME));
        url.setUserName(QString::fromStdString(account1.email));
        url.setPassword(QString::fromStdString(account1Password));
        ASSERT_TRUE(httpClient.doGet(url));

        auto msgBody = httpClient.fetchMessageBodyBuffer();
        nx_http::FusionRequestResult requestResult =
            QJson::deserialized<nx_http::FusionRequestResult>(msgBody);

        ASSERT_TRUE(httpClient.response() != nullptr);
        ASSERT_EQ(nx_http::StatusCode::ok, httpClient.response()->statusLine.statusCode);
        ASSERT_EQ(nx_http::FusionRequestErrorClass::unauthorized, requestResult.errorClass);
        ASSERT_EQ(
            QnLexical::serialized(api::ResultCode::accountNotActivated),
            requestResult.resultCode);
        ASSERT_EQ(
            (int)api::ResultCode::accountNotActivated,
            requestResult.errorDetail);
    }
}

TEST_F(CdbFunctionalTest, api_conventions_usingPostMethod)
{
    const QByteArray testData =
        "{\"fullName\": \"a k\", \"passwordHa1\": \"5f6291102209098cf5432a415e26d002\", "
        "\"email\": \"andreyk07@gmail.com\", \"customization\": \"default\"}";

    bool success = false;
    auto accountData = QJson::deserialized<api::AccountData>(
        testData, api::AccountData(), &success);
    ASSERT_TRUE(success);

    startAndWaitUntilStarted();

    auto client = nx_http::AsyncHttpClient::create();
    QUrl url;
    url.setHost(endpoint().address.toString());
    url.setPort(endpoint().port);
    url.setScheme("http");
    url.setPath("/account/register");
    std::promise<void> donePromise;
    auto doneFuture = donePromise.get_future();
    QObject::connect(
        client.get(), &nx_http::AsyncHttpClient::done,
        client.get(), [&donePromise](nx_http::AsyncHttpClientPtr /*client*/) {
            donePromise.set_value();
        },
        Qt::DirectConnection);
    client->doPost(url, "application/json", testData);

    doneFuture.wait();
    ASSERT_TRUE(client->response() != nullptr);
    ASSERT_EQ(nx_http::StatusCode::ok, client->response()->statusLine.statusCode);
}

TEST_F(CdbFunctionalTest, api_conventions_jsonInUnauthorizedResponse)
{
    startAndWaitUntilStarted();

    for (int i = 0; i < 2; ++i)
    {
        nx_http::HttpClient client;
        QUrl url;
        url.setHost(endpoint().address.toString());
        url.setPort(endpoint().port);
        url.setScheme("http");
        url.setPath("/account/get");
        if (i == 1)
        {
            url.setUserName("invalid");
            url.setPassword("password");
        }
        ASSERT_TRUE(client.doGet(url));
        ASSERT_TRUE(client.response() != nullptr);
        ASSERT_EQ(
            nx_http::StatusCode::unauthorized,
            client.response()->statusLine.statusCode);

        nx_http::BufferType msgBody;
        while (!client.eof())
            msgBody += client.fetchMessageBodyBuffer();

        ASSERT_FALSE(msgBody.isEmpty());
        nx_http::FusionRequestResult requestResult =
            QJson::deserialized<nx_http::FusionRequestResult>(msgBody);

        ASSERT_EQ(
            nx_http::FusionRequestErrorClass::unauthorized,
            requestResult.errorClass);
        ASSERT_EQ(
            QnLexical::serialized(api::ResultCode::notAuthorized),
            requestResult.resultCode);
    }
}

TEST_F(CdbFunctionalTest, api_conventions_jsonInOkResponse)
{
    startAndWaitUntilStarted();

    api::AccountData account1;
    std::string account1Password;
    auto result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(result, api::ResultCode::ok);

    //changing password
    std::string account1NewPassword = account1Password + "new";
    api::AccountUpdateData update;
    update.passwordHa1 = nx_http::calcHa1(
        account1.email.c_str(),
        moduleInfo().realm.c_str(),
        account1NewPassword.c_str()).constData();
    update.fullName = account1.fullName + "new";
    update.customization = account1.customization + "new";

    QUrlQuery urlQuery;
    nx::cdb::api::serializeToUrlQuery(update, &urlQuery);

    nx_http::HttpClient client;
    QUrl url;
    url.setHost(endpoint().address.toString());
    url.setPort(endpoint().port);
    url.setScheme("http");
    url.setPath("/account/update");
    url.setUserName(QString::fromStdString(account1.email));
    url.setPassword(QString::fromStdString(account1Password));
    url.setQuery(std::move(urlQuery));
    ASSERT_TRUE(client.doGet(url));
    ASSERT_TRUE(client.response() != nullptr);
    ASSERT_EQ(
        nx_http::StatusCode::ok,
        client.response()->statusLine.statusCode);

    nx_http::BufferType msgBody;
    while (!client.eof())
        msgBody += client.fetchMessageBodyBuffer();

    ASSERT_FALSE(msgBody.isEmpty());
    nx_http::FusionRequestResult requestResult =
        QJson::deserialized<nx_http::FusionRequestResult>(msgBody);

    ASSERT_EQ(
        nx_http::FusionRequestErrorClass::noError,
        requestResult.errorClass);
    ASSERT_EQ(
        QnLexical::serialized(nx_http::FusionRequestErrorDetail::noError),
        requestResult.resultCode);

    result = getAccount(account1.email, account1NewPassword, &account1);
    ASSERT_EQ(result, api::ResultCode::ok);
}

}   //cdb
}   //nx
