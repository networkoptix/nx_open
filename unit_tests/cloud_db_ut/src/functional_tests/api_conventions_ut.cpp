/**********************************************************
* Nov 23, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <utils/common/model_functions.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/http/server/fusion_request_result.h>

#include "test_setup.h"
#include "version.h"


namespace nx {
namespace cdb {

TEST_F(CdbFunctionalTest, api_conventions)
{
    //waiting for cloud_db initialization
    waitUntilStarted();

    api::AccountData account1;
    std::string account1Password;
    api::AccountActivationCode activationCode;
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
        ASSERT_EQ(nx_http::FusionRequestErrorClass::badRequest, requestResult.resultCode);
        ASSERT_EQ(
            (int)nx_http::FusionRequestResult::ecDeserializationError,
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
        ASSERT_EQ(nx_http::FusionRequestErrorClass::unauthorized, requestResult.resultCode);
        ASSERT_EQ(
            (int)api::ResultCode::forbidden,
            requestResult.errorDetail);
    }
}

}   //cdb
}   //nx
