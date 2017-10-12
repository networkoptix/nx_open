#include <memory>

#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/cdb/managers/vms_gateway.h>
#include <nx/cloud/cdb/settings.h>

namespace nx {
namespace cdb {
namespace test {

class VmsGateway:
    public ::testing::Test
{
protected:    
    void givenUnreachableVms()
    {
        m_mediaserverEmulator.reset();
    }

    void whenIssueMergeRequest()
    {
        using namespace std::placeholders;

        m_vmsGateway->merge(
            m_systemId,
            std::bind(&VmsGateway::saveMergeResult, this, _1));
    }

    void thenMergeRequestIsReceivedByServer()
    {
        m_vmsApiRequests.pop();
    }

    void thenMergeRequestSucceeded()
    {
        thenMergeRequestResultIs(VmsResultCode::ok);
    }

    void thenMergeRequestResultIs(VmsResultCode resultCode)
    {
        ASSERT_EQ(resultCode, m_vmsRequestResults.pop().resultCode);
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
    std::unique_ptr<TestHttpServer> m_mediaserverEmulator;
    nx::utils::SyncQueue<nx_http::Request> m_vmsApiRequests;
    nx::utils::SyncQueue<VmsRequestResult> m_vmsRequestResults;
    conf::Settings m_settings;
    std::unique_ptr<cdb::VmsGateway> m_vmsGateway;
    std::string m_systemId;
    boost::optional<nx_http::StatusCode::Value> m_forcedHttpResponseStatus;

    virtual void SetUp() override
    {
        using namespace std::placeholders;

        m_mediaserverEmulator = std::make_unique<TestHttpServer>();
        m_mediaserverEmulator->registerRequestProcessorFunc(
            "/gateway/{systemId}/api/mergeSystems",
            std::bind(&VmsGateway::vmsApiRequestStub, this, _1, _2, _3, _4, _5));
        ASSERT_TRUE(m_mediaserverEmulator->bindAndListen());

        std::string vmsUrl = lm("http://%1/gateway/{systemId}/")
            .args(m_mediaserverEmulator->serverAddress()).toStdString();
        std::array<const char*, 2> args{"-vmsGateway/url", vmsUrl.c_str()};

        m_settings.load((int)args.size(), args.data());
        m_vmsGateway = std::make_unique<cdb::VmsGateway>(m_settings);

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
};

TEST_F(VmsGateway, invokes_merge_request)
{
    whenIssueMergeRequest();
    
    thenMergeRequestIsReceivedByServer();
    thenMergeRequestSucceeded();
}

// TEST_F(VmsGateway, merge_request_authentication)

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
