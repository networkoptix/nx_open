#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/server/fusion_request_result.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/app_info.h>

#include <nx/cloud/cdb/client/data/account_data.h>
#include <nx/cloud/cdb/client/data/types.h>

#include "test_setup.h"

namespace nx {
namespace cdb {
namespace test {

class ApiConventions:
    public CdbFunctionalTest
{
protected:
    using DetailedApiRequestResult = std::tuple<
        nx::network::http::StatusCode::Value,
        nx::network::http::FusionRequestErrorClass,
        api::ResultCode>;

    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());
    }

    void assertRequestWithoutCredentialsIsRejectedWith(
        api::ResultCode resultCode)
    {
        DetailedApiRequestResult result;
        issueRequest("/cdb/account/get", std::string(), std::string(), &result);

        ASSERT_EQ(nx::network::http::StatusCode::unauthorized, std::get<0>(result));
        ASSERT_EQ(nx::network::http::FusionRequestErrorClass::unauthorized, std::get<1>(result));
        ASSERT_EQ(resultCode, std::get<2>(result));
    }

    void assertRequestWithInvalidCredentialsIsRejectedWith(
        api::ResultCode resultCode)
    {
        DetailedApiRequestResult result;
        issueRequest("/cdb/account/get", "invalid", "credentials", &result);

        ASSERT_EQ(nx::network::http::StatusCode::unauthorized, std::get<0>(result));
        ASSERT_EQ(nx::network::http::FusionRequestErrorClass::unauthorized, std::get<1>(result));
        ASSERT_EQ(resultCode, std::get<2>(result));
    }

    void issueRequest(
        const std::string& path,
        const std::string& username,
        const std::string& password,
        DetailedApiRequestResult* result)
    {
        nx::network::http::HttpClient client;
        nx::utils::Url url;
        url.setHost(endpoint().address.toString());
        url.setPort(endpoint().port);
        url.setScheme("http");
        url.setPath(path.c_str());
        if (!username.empty())
            url.setUserName(username.c_str());
        if (!password.empty())
            url.setPassword(password.c_str());
        ASSERT_TRUE(client.doGet(url));
        ASSERT_TRUE(client.response() != nullptr);
        const nx::network::http::StatusCode::Value httpStatusCode =
            static_cast<nx::network::http::StatusCode::Value>(
                client.response()->statusLine.statusCode);

        nx::network::http::BufferType msgBody;
        while (!client.eof())
            msgBody += client.fetchMessageBodyBuffer();

        ASSERT_FALSE(msgBody.isEmpty());
        nx::network::http::FusionRequestResult requestResult =
            QJson::deserialized<nx::network::http::FusionRequestResult>(msgBody);

        const auto errorClass = requestResult.errorClass;
        const auto resultCode =
            QnLexical::deserialized<api::ResultCode>(requestResult.resultCode);

        *result = std::make_tuple(httpStatusCode, errorClass, resultCode);
    }
};

TEST_F(ApiConventions, general)
{
    api::AccountData account1;
    std::string account1Password;
    api::AccountConfirmationCode activationCode;
    auto result = addAccount(&account1, &account1Password, &activationCode);
    ASSERT_EQ(result, api::ResultCode::ok);

    {
        //missing required parameter
        nx::network::http::HttpClient httpClient;
        nx::utils::Url url(lit("http://%1:%2/cdb/system/bind?name=esadfwer").arg(endpoint().address.toString()).arg(endpoint().port));
        url.setUserName(QString::fromStdString(account1.email));
        url.setPassword(QString::fromStdString(account1Password));
        ASSERT_TRUE(httpClient.doGet(url));

        auto msgBody = httpClient.fetchMessageBodyBuffer();
        nx::network::http::FusionRequestResult requestResult =
            QJson::deserialized<nx::network::http::FusionRequestResult>(msgBody);

        ASSERT_TRUE(httpClient.response() != nullptr);
        ASSERT_EQ(nx::network::http::StatusCode::badRequest, httpClient.response()->statusLine.statusCode);
        ASSERT_EQ(nx::network::http::FusionRequestErrorClass::badRequest, requestResult.errorClass);
        ASSERT_EQ(
            QnLexical::serialized(nx::network::http::FusionRequestErrorDetail::deserializationError),
            requestResult.resultCode);
        ASSERT_EQ(
            (int)nx::network::http::FusionRequestErrorDetail::deserializationError,
            requestResult.errorDetail);
    }

    {
        //operation forbidden for account in this state
        nx::network::http::HttpClient httpClient;
        nx::utils::Url url(lit("http://%1:%2/cdb/system/bind?name=esadfwer&customization=%3").
            arg(endpoint().address.toString()).arg(endpoint().port).arg(nx::utils::AppInfo::customizationName()));
        url.setUserName(QString::fromStdString(account1.email));
        url.setPassword(QString::fromStdString(account1Password));
        ASSERT_TRUE(httpClient.doGet(url));

        auto msgBody = httpClient.fetchMessageBodyBuffer();
        nx::network::http::FusionRequestResult requestResult =
            QJson::deserialized<nx::network::http::FusionRequestResult>(msgBody);

        ASSERT_TRUE(httpClient.response() != nullptr);
        ASSERT_EQ(nx::network::http::StatusCode::forbidden, httpClient.response()->statusLine.statusCode);
        ASSERT_EQ(nx::network::http::FusionRequestErrorClass::unauthorized, requestResult.errorClass);
        ASSERT_EQ(
            QnLexical::serialized(api::ResultCode::accountNotActivated),
            requestResult.resultCode);
        ASSERT_EQ(
            (int)api::ResultCode::accountNotActivated,
            requestResult.errorDetail);
    }
}

TEST_F(ApiConventions, using_post_method)
{
    const QByteArray testData =
        "{\"fullName\": \"a k\", \"passwordHa1\": \"5f6291102209098cf5432a415e26d002\", "
        "\"email\": \"abrakadabra@gmail.com\", \"customization\": \"default\"}";

    bool success = false;
    auto accountData = QJson::deserialized<api::AccountData>(
        testData, api::AccountData(), &success);
    ASSERT_TRUE(success);

    auto client = nx::network::http::AsyncHttpClient::create();
    nx::utils::Url url;
    url.setHost(endpoint().address.toString());
    url.setPort(endpoint().port);
    url.setScheme("http");
    url.setPath("/cdb/account/register");
    nx::utils::promise<void> donePromise;
    auto doneFuture = donePromise.get_future();
    QObject::connect(
        client.get(), &nx::network::http::AsyncHttpClient::done,
        client.get(), [&donePromise](nx::network::http::AsyncHttpClientPtr /*client*/) {
            donePromise.set_value();
        },
        Qt::DirectConnection);
    client->doPost(url, "application/json", testData);

    doneFuture.wait();
    ASSERT_TRUE(client->response() != nullptr);
    ASSERT_EQ(nx::network::http::StatusCode::ok, client->response()->statusLine.statusCode);
}

TEST_F(ApiConventions, json_in_unauthorized_response)
{
    assertRequestWithoutCredentialsIsRejectedWith(api::ResultCode::notAuthorized);
    assertRequestWithInvalidCredentialsIsRejectedWith(api::ResultCode::badUsername);
}

TEST_F(ApiConventions, api_conventions_ok)
{
    api::AccountData account1;
    std::string account1Password;
    auto result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(api::ResultCode::ok, result);

    nx::network::http::HttpClient client;
    client.setResponseReadTimeout(std::chrono::minutes(1));
    for (int i = 0; i < 27; ++i)
    {
        nx::utils::Url url =
            nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
                .setEndpoint(endpoint()).setPath("/cdb/account/get")
                .setUserName(QString::fromStdString(account1.email))
                .setPassword(QString::fromStdString(account1Password)).toUrl();
        ASSERT_TRUE(client.doGet(url));
        ASSERT_TRUE(client.response() != nullptr);
        ASSERT_EQ(
            nx::network::http::StatusCode::ok,
            client.response()->statusLine.statusCode);

        nx::network::http::BufferType msgBody;
        while (!client.eof())
            msgBody += client.fetchMessageBodyBuffer();

        ASSERT_FALSE(msgBody.isEmpty());
    }
}

TEST_F(ApiConventions, json_in_ok_response)
{
    api::AccountData account1;
    std::string account1Password;
    auto result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(result, api::ResultCode::ok);

    //changing password
    std::string account1NewPassword = account1Password + "new";
    api::AccountUpdateData update;
    update.passwordHa1 = nx::network::http::calcHa1(
        account1.email.c_str(),
        moduleInfo().realm.c_str(),
        account1NewPassword.c_str()).constData();
    update.fullName = account1.fullName + "new";
    update.customization = account1.customization + "new";

    QUrlQuery urlQuery;
    nx::cdb::api::serializeToUrlQuery(update, &urlQuery);

    nx::network::http::HttpClient client;
    nx::utils::Url url;
    url.setHost(endpoint().address.toString());
    url.setPort(endpoint().port);
    url.setScheme("http");
    url.setPath("/cdb/account/update");
    url.setUserName(QString::fromStdString(account1.email));
    url.setPassword(QString::fromStdString(account1Password));
    url.setQuery(std::move(urlQuery));
    ASSERT_TRUE(client.doGet(url));
    ASSERT_TRUE(client.response() != nullptr);
    ASSERT_EQ(
        nx::network::http::StatusCode::ok,
        client.response()->statusLine.statusCode);

    nx::network::http::BufferType msgBody;
    while (!client.eof())
        msgBody += client.fetchMessageBodyBuffer();

    ASSERT_FALSE(msgBody.isEmpty());
    nx::network::http::FusionRequestResult requestResult =
        QJson::deserialized<nx::network::http::FusionRequestResult>(msgBody);

    ASSERT_EQ(
        nx::network::http::FusionRequestErrorClass::noError,
        requestResult.errorClass);
    ASSERT_EQ(
        QnLexical::serialized(nx::network::http::FusionRequestErrorDetail::ok),
        requestResult.resultCode);

    result = getAccount(account1.email, account1NewPassword, &account1);
    ASSERT_EQ(result, api::ResultCode::ok);
}

} // namespace test
} // namespace cdb
} // namespace nx
