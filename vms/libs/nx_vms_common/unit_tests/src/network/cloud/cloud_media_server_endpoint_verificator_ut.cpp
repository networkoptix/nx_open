// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_global.h>
#include <nx/utils/thread/sync_queue.h>

#include <network/cloud/cloud_media_server_endpoint_verificator.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/network/rest/result.h>

namespace test {

using VerificationResult =
    nx::network::cloud::tcp::AbstractEndpointVerificator::VerificationResult;

class CloudMediaServerEndpointVerificator:
    public ::testing::Test
{
public:
    ~CloudMediaServerEndpointVerificator()
    {
        if (m_verificator)
            m_verificator->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        m_cloudSystemId = nx::Uuid::createUuid().toSimpleStdString();

        m_moduleInformation.id = nx::Uuid::createUuid();
        m_moduleInformation.realm = nx::network::AppInfo::realm().c_str();
        m_moduleInformation.cloudHost = nx::network::SocketGlobals::cloud().cloudHost().c_str();
        m_moduleInformation.cloudSystemId = m_cloudSystemId.c_str();

        m_httpServer.registerRequestProcessorFunc(
            "/api/moduleInformation",
            [this](auto&&... args) { provideModuleInformation(std::forward<decltype(args)>(args)...); },
            nx::network::http::Method::get);

        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    void removeCloudSystemIdFromModuleInformation()
    {
        m_moduleInformation.cloudSystemId.clear();
    }

    void setRandomCloudSystemIdInModuleInformation()
    {
        m_moduleInformation.cloudSystemId = nx::Uuid::createUuid().toSimpleString();
    }

    void setRandomIdInModuleInformation()
    {
        m_moduleInformation.id = nx::Uuid::createUuid();
    }

    void verifyByCloudSystemId()
    {
        m_hostNameToVerify = m_cloudSystemId;
    }

    void verifyByFullServerName()
    {
        m_hostNameToVerify = m_moduleInformation.id.toSimpleStdString() + "." + m_cloudSystemId;
    }

    void whenVerifyEndpoint()
    {
        m_verificator = std::make_unique<::CloudMediaServerEndpointVerificator>("sessionId");
        m_verificator->verifyHost(
            m_httpServer.serverAddress(),
            nx::network::AddressEntry(nx::network::AddressType::cloud, m_hostNameToVerify),
            [this](auto&&... args) { m_results.push(std::forward<decltype(args)>(args)...); });
    }

    void thenVerificationResultIs(VerificationResult expected)
    {
        const auto result = m_results.pop();
        ASSERT_EQ(expected, result);
    }

    void thenVerificationSucceeded()
    {
        thenVerificationResultIs(VerificationResult::passed);
    }

private:
    void provideModuleInformation(
        nx::network::http::RequestContext /*request*/,
        nx::network::http::RequestProcessedHandler handler)
    {
        nx::network::rest::JsonResult apiResult;
        apiResult.setReply(m_moduleInformation);

        nx::network::http::RequestResult httpResult(nx::network::http::StatusCode::ok);
        httpResult.body = std::make_unique<nx::network::http::BufferSource>(
            "application/json",
            QJson::serialized(apiResult));
        handler(std::move(httpResult));
    }

private:
    nx::network::http::TestHttpServer m_httpServer;
    std::string m_cloudSystemId;
    nx::vms::api::ModuleInformation m_moduleInformation;
    std::unique_ptr<::CloudMediaServerEndpointVerificator> m_verificator;
    nx::utils::SyncQueue<VerificationResult> m_results;
    std::string m_hostNameToVerify;
};

TEST_F(CloudMediaServerEndpointVerificator, target_system_is_verified)
{
    verifyByCloudSystemId();
    whenVerifyEndpoint();
    thenVerificationSucceeded();
}

TEST_F(CloudMediaServerEndpointVerificator, target_server_is_verified)
{
    verifyByFullServerName();
    whenVerifyEndpoint();
    thenVerificationSucceeded();
}

TEST_F(CloudMediaServerEndpointVerificator, server_without_cloud_system_id_is_not_verified)
{
    verifyByCloudSystemId();
    removeCloudSystemIdFromModuleInformation();

    whenVerifyEndpoint();

    thenVerificationResultIs(VerificationResult::notPassed);
}

TEST_F(CloudMediaServerEndpointVerificator, server_with_unknown_cloud_system_id_is_not_verified)
{
    verifyByCloudSystemId();
    setRandomCloudSystemIdInModuleInformation();

    whenVerifyEndpoint();

    thenVerificationResultIs(VerificationResult::notPassed);
}

TEST_F(CloudMediaServerEndpointVerificator, other_server_within_same_system_is_not_verified)
{
    verifyByFullServerName();
    setRandomIdInModuleInformation();

    whenVerifyEndpoint();

    thenVerificationResultIs(VerificationResult::notPassed);
}

} // namespace test
