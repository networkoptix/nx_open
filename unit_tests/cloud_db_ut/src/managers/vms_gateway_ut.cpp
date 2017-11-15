#include <memory>

#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/cdb/managers/vms_gateway.h>
#include <nx/cloud/cdb/settings.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>
#include <nx/cloud/cdb/api/cloud_nonce.h>

#include <api/auth_util.h>

#include "account_manager_stub.h"

namespace nx {
namespace cdb {
namespace test {

class VmsGateway:
    public ::testing::Test
{
public:
    VmsGateway():
        m_idOfSystemToMergeTo(QnUuid::createUuid().toSimpleString().toStdString())
    {
    }

protected:
    void givenUnreachableVms()
    {
        m_mediaserverEmulator.reset();
    }

    void givenVmsWithAuthenticationEnabled()
    {
        m_mediaserverEmulator->setAuthenticationEnabled(true);
        m_mediaserverEmulator->registerUserCredentials(
            m_ownerAccount.email.c_str(), m_ownerAccount.password.c_str());
    }

    void whenIssueMergeRequest()
    {
        using namespace std::placeholders;

        m_vmsGateway->merge(
            m_ownerAccount.email,
            m_systemId,
            m_idOfSystemToMergeTo,
            std::bind(&VmsGateway::saveMergeResult, this, _1));
    }

    void thenMergeRequestIsReceivedByServer()
    {
        m_prevReceivedVmsApiRequest = m_vmsApiRequests.pop();
    }

    void andMergeRequestParametersAreExpected()
    {
        const auto mergeRequestData =
            QJson::deserialized<MergeSystemData>(m_prevReceivedVmsApiRequest->messageBody);

        ASSERT_FALSE(mergeRequestData.mergeOneServer);
        ASSERT_TRUE(mergeRequestData.takeRemoteSettings);
        ASSERT_FALSE(mergeRequestData.ignoreIncompatible);
        assertAuthKeyIsValid(
            mergeRequestData.getKey.toUtf8(),
            nx_http::Method::get,
            "/api/mergeSystems");
        assertAuthKeyIsValid(
            mergeRequestData.postKey.toUtf8(),
            nx_http::Method::post,
            "/api/mergeSystems");
        QUrl remoteSystemUrl(mergeRequestData.url);
        ASSERT_EQ(nx_http::kSecureUrlSchemeName, remoteSystemUrl.scheme());
        ASSERT_EQ(m_idOfSystemToMergeTo, remoteSystemUrl.host().toStdString());
    }

    void thenMergeRequestSucceeded()
    {
        thenMergeRequestResultIs(VmsResultCode::ok);
    }

    void thenMergeRequestResultIs(VmsResultCode resultCode)
    {
        ASSERT_EQ(resultCode, m_vmsRequestResults.pop().resultCode);
    }

    void thenRequestIsAuthenticatedWithCredentialsSpecified()
    {
        auto vmsApiRequest = m_vmsApiRequests.pop();

        const auto authorizationHeaderStr = nx_http::getHeaderValue(
            vmsApiRequest.headers, nx_http::header::Authorization::NAME);
        ASSERT_FALSE(authorizationHeaderStr.isEmpty());
        nx_http::header::Authorization authorization;
        ASSERT_TRUE(authorization.parse(authorizationHeaderStr));
        ASSERT_EQ(m_ownerAccount.email, authorization.userid().toStdString());
    }

    void assertVmsResponseResultsIn(
        nx_http::StatusCode::Value vmsHttpResponseStatusCode,
        VmsResultCode expectedCode)
    {
        forceVmsApiResponseStatus(vmsHttpResponseStatusCode);
        whenIssueMergeRequest();
        thenMergeRequestResultIs(expectedCode);
    }

private:
    AccountManagerStub m_accountManagerStub;
    std::unique_ptr<TestHttpServer> m_mediaserverEmulator;
    nx::utils::SyncQueue<nx_http::Request> m_vmsApiRequests;
    boost::optional<nx_http::Request> m_prevReceivedVmsApiRequest;
    nx::utils::SyncQueue<VmsRequestResult> m_vmsRequestResults;
    conf::Settings m_settings;
    std::unique_ptr<cdb::VmsGateway> m_vmsGateway;
    std::string m_systemId;
    std::string m_idOfSystemToMergeTo;
    boost::optional<nx_http::StatusCode::Value> m_forcedHttpResponseStatus;
    AccountWithPassword m_ownerAccount;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_ownerAccount = BusinessDataGenerator::generateRandomAccount();
        m_accountManagerStub.addAccount(m_ownerAccount);

        m_mediaserverEmulator = std::make_unique<TestHttpServer>();
        m_mediaserverEmulator->registerRequestProcessorFunc(
            "/gateway/{systemId}/api/mergeSystems",
            std::bind(&VmsGateway::vmsApiRequestStub, this, _1, _2, _3, _4, _5));
        ASSERT_TRUE(m_mediaserverEmulator->bindAndListen());

        std::string vmsUrl = lm("http://%1/gateway/{systemId}/")
            .args(m_mediaserverEmulator->serverAddress()).toStdString();
        std::array<const char*, 2> args{"-vmsGateway/url", vmsUrl.c_str()};

        m_settings.load((int)args.size(), args.data());
        m_vmsGateway = std::make_unique<cdb::VmsGateway>(m_settings, m_accountManagerStub);

        m_systemId = QnUuid::createUuid().toSimpleByteArray().toStdString();
    }

    void vmsApiRequestStub(
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request request,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler)
    {
        m_vmsApiRequests.push(std::move(request));

        if (m_forcedHttpResponseStatus)
            return completionHandler(*m_forcedHttpResponseStatus);

        QnJsonRestResult response;
        response.error = QnRestResult::Error::NoError;

        nx_http::RequestResult requestResult(nx_http::StatusCode::ok);
        requestResult.dataSource = std::make_unique<nx_http::BufferSource>(
            "application/json",
            QJson::serialized(response));

        completionHandler(std::move(requestResult));
    }

    void saveMergeResult(VmsRequestResult vmsRequestResult)
    {
        m_vmsRequestResults.push(std::move(vmsRequestResult));
    }

    void forceVmsApiResponseStatus(nx_http::StatusCode::Value httpStatusCode)
    {
        m_forcedHttpResponseStatus = httpStatusCode;
    }

    void assertAuthKeyIsValid(
        const nx::String& authKeyStr,
        nx_http::Method::ValueType httpMethod,
        const nx::String& apiRequest)
    {
        AuthKey authKey;
        ASSERT_TRUE(authKey.parse(authKeyStr));
        ASSERT_EQ(m_ownerAccount.email, authKey.username.toStdString());
        ASSERT_TRUE(api::isNonceValidForSystem(
            authKey.nonce.toStdString(), m_idOfSystemToMergeTo));
        ASSERT_TRUE(authKey.verify(
            nx_http::PasswordAuthToken(m_ownerAccount.password.c_str()),
            httpMethod,
            apiRequest));
    }
};

TEST_F(VmsGateway, invokes_merge_request)
{
    whenIssueMergeRequest();

    thenMergeRequestIsReceivedByServer();
    thenMergeRequestSucceeded();
}

TEST_F(VmsGateway, vms_merge_request_is_authenticated)
{
    givenVmsWithAuthenticationEnabled();

    whenIssueMergeRequest();

    thenMergeRequestSucceeded();
    thenRequestIsAuthenticatedWithCredentialsSpecified();
}

TEST_F(VmsGateway, merge_request_parameteres_are_correct)
{
    whenIssueMergeRequest();

    thenMergeRequestIsReceivedByServer();
    andMergeRequestParametersAreExpected();
}

TEST_F(VmsGateway, proper_error_is_reported_when_vms_is_unreachable)
{
    givenUnreachableVms();
    whenIssueMergeRequest();
    thenMergeRequestResultIs(VmsResultCode::networkError);
}

TEST_F(VmsGateway, proper_error_is_reported_when_vms_rejects_request)
{
    assertVmsResponseResultsIn(
        nx_http::StatusCode::serviceUnavailable,
        VmsResultCode::unreachable);

    assertVmsResponseResultsIn(
        nx_http::StatusCode::notFound,
        VmsResultCode::unreachable);

    assertVmsResponseResultsIn(
        nx_http::StatusCode::badRequest,
        VmsResultCode::invalidData);

    assertVmsResponseResultsIn(
        nx_http::StatusCode::unauthorized,
        VmsResultCode::forbidden);

    assertVmsResponseResultsIn(
        nx_http::StatusCode::forbidden,
        VmsResultCode::forbidden);
}

} // namespace test
} // namespace cdb
} // namespace nx
