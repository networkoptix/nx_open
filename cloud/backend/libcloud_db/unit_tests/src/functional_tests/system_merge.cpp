#include <future>
#include <optional>
#include <set>

#include <gtest/gtest.h>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/server/fusion_request_result.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/std/algorithm.h>

#include <nx/cloud/db/cloud_db_service.h>
#include <nx/cloud/db/client/cdb_request_path.h>
#include <nx/cloud/db/client/data/types.h>
#include <nx/cloud/db/controller.h>
#include <nx/cloud/db/managers/system_health_info_provider.h>

#include <rest/server/json_rest_result.h>

#include "test_setup.h"

namespace nx::cloud::db {
namespace test {

namespace {

class SystemHealthInfoProviderStub:
    public AbstractSystemHealthInfoProvider
{
public:
    virtual bool isSystemOnline(const std::string& systemId) const override
    {
        return m_onlineSystems.find(systemId) != m_onlineSystems.end();
    }

    virtual void getSystemHealthHistory(
        const AuthorizationInfo& /*authzInfo*/,
        data::SystemId /*systemId*/,
        std::function<void(api::Result, api::SystemHealthHistory)> completionHandler) override
    {
        m_aioThreadBinder.post(
            std::bind(completionHandler, api::ResultCode::ok, api::SystemHealthHistory()));
    }

    void forceSystemOnline(const std::string& systemId)
    {
        m_onlineSystems.insert(systemId);
    }

private:
    nx::network::aio::BasicPollable m_aioThreadBinder;
    std::set<std::string> m_onlineSystems;
};

} // namespace

class SystemMerge:
    public CdbFunctionalTest
{
    using base_type = CdbFunctionalTest;

public:
    SystemMerge()
    {
        m_factoryBak = SystemHealthInfoProviderFactory::instance().setCustomFunc(
            [this](auto&&... args) { return createSystemHealthInfoProvider(std::move(args)...); });

        m_vmsResponse.error = QnRestResult::Error::NoError;
    }

    ~SystemMerge()
    {
        SystemHealthInfoProviderFactory::instance().setCustomFunc(std::move(m_factoryBak));
    }

protected:
    virtual void SetUp() override
    {
        using namespace std::placeholders;

        base_type::SetUp();

        m_vmsGatewayEmulator.registerRequestProcessorFunc(
            "/gateway/{systemId}/api/mergeSystems",
            std::bind(&SystemMerge::vmsApiRequestStub, this, _1, _2));
        ASSERT_TRUE(m_vmsGatewayEmulator.bindAndListen());

        addArg("-vmsGateway/url", lm("http://%1/gateway/{systemId}/")
            .args(m_vmsGatewayEmulator.serverAddress()).toStdString().c_str());

        ASSERT_TRUE(startAndWaitUntilStarted());

        m_ownerAccount = addActivatedAccount2();

        m_vmsGatewayEmulator.setAuthenticationEnabled(true);
        m_vmsGatewayEmulator.registerUserCredentials(
            m_ownerAccount.email.c_str(),
            m_ownerAccount.password.c_str());
    }

    void givenVmsThatReturnsLogicalError(const std::string& errorText)
    {
        m_vmsResponse.error = QnRestResult::Error::CantProcessRequest;
        m_vmsResponse.errorString = errorText.c_str();
    }

    void givenTwoSystemsWithSameOwner()
    {
        m_masterSystem = addRandomSystemToAccount(m_ownerAccount);
        m_slaveSystem = addRandomSystemToAccount(m_ownerAccount);
    }

    void givenTwoOnlineSystemsWithSameOwner()
    {
        givenTwoSystemsWithSameOwner();

        m_systemHealthInfoProviderStub->forceSystemOnline(m_masterSystem.id);
        m_systemHealthInfoProviderStub->forceSystemOnline(m_slaveSystem.id);
    }

    void givenTwoOnlineSystemsWithDifferentOwners()
    {
        givenTwoOnlineSystemsWithSameOwner();

        AccountWithPassword otherAccount = addActivatedAccount2();
        m_slaveSystem = addRandomSystemToAccount(otherAccount);

        m_systemHealthInfoProviderStub->forceSystemOnline(m_slaveSystem.id);
    }

    void givenTwoOnlineSystemsBeingMerged()
    {
        givenTwoOnlineSystemsWithSameOwner();
        whenMergeSystems();
        thenMergeSucceeded();
    }

    void givenSystemAfterMerge()
    {
        givenTwoOnlineSystemsBeingMerged();
        whenMergeIsCompleted();
        waitUntilSlaveSystemIsRemoved();
    }

    void whenMergeSystems()
    {
        // Sending HTTP request because we need access to the response error text.
        // Otherwise, it could be as simple as:
        // m_prevResult.code = mergeSystems(m_ownerAccount, m_masterSystem.id, m_slaveSystem.id);

        const auto url = nx::network::url::Builder()
            .setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(endpoint()).setPath(
                nx::network::http::rest::substituteParameters(
                    kSystemsMergedToASpecificSystem, {m_masterSystem.id}));

        nx::network::http::HttpClient httpClient;
        httpClient.setUserName(m_ownerAccount.email.c_str());
        httpClient.setUserPassword(m_ownerAccount.password.c_str());
        httpClient.setResponseReadTimeout(nx::network::kNoTimeout);
        httpClient.setMessageBodyReadTimeout(nx::network::kNoTimeout);
        ASSERT_TRUE(httpClient.doPost(
            url,
            "application/json",
            QJson::serialized(api::SystemId(m_slaveSystem.id))));

        const auto resultCodeStr = nx::network::http::getHeaderValue(
            httpClient.response()->headers,
            Qn::API_RESULT_CODE_HEADER_NAME);
        const auto responseBody = httpClient.fetchEntireMessageBody();

        m_prevResult.code = QnLexical::deserialized<api::ResultCode>(resultCodeStr);
        m_prevResult.description = 
            QJson::deserialized<nx::network::http::FusionRequestResult>(*responseBody)
                .errorText.toStdString();
    }

    void whenSlaveSystemFailsEveryRequest()
    {
        m_vmsApiResult = nx::network::http::StatusCode::internalServerError;
    }

    void whenMergeIsCompleted()
    {
        nx::vms::api::SystemMergeHistoryRecord mergeHistoryRecord;
        mergeHistoryRecord.mergedSystemCloudId = m_slaveSystem.id.c_str();
        mergeHistoryRecord.sign(m_slaveSystem.authKey.c_str());

        std::promise<api::ResultCode> done;
        moduleInstance()->impl()->controller().systemMergeManager().processMergeHistoryRecord(
            mergeHistoryRecord,
            [&done](api::Result result) { done.set_value(result.code); });
        ASSERT_EQ(api::ResultCode::ok, done.get_future().get());
    }

    void thenResultCodeIs(api::ResultCode resultCode)
    {
        ASSERT_EQ(resultCode, m_prevResult.code);
    }

    void thenMergeSucceeded()
    {
        thenResultCodeIs(api::ResultCode::ok);
    }

    void thenMergeFailed()
    {
        ASSERT_NE(api::ResultCode::ok, m_prevResult.code);
    }

    void andResponseErrorTextIs(const std::string& expected)
    {
        ASSERT_EQ(expected, m_prevResult.description);
    }

    void andSlaveSystemIsMovedToBeingMergedState()
    {
        api::SystemDataEx system;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystem(m_ownerAccount.email, m_ownerAccount.password, m_slaveSystem.id, &system));
        ASSERT_EQ(api::SystemStatus::beingMerged, system.status);
    }

    void thenVmsApiMergeRequestHasBeenIssuedToSlaveSystem()
    {
        m_prevVmsApiRequest = m_vmsApiRequests.pop();

        auto requestPath = m_prevVmsApiRequest->requestLine.url.path();
        auto requestPathItems = requestPath.split('/', QString::SkipEmptyParts);

        requestPathItems.pop_front(); //< Skipping api prefix.

        ASSERT_EQ(m_slaveSystem.id, requestPathItems.front().toStdString());
        requestPathItems.pop_front();
    }

    void thenVmsRequestIsAuthenticatedWithOwnerCredentials()
    {
        m_prevVmsApiRequest = m_vmsApiRequests.pop();
        const auto authorizationHeaderStr = nx::network::http::getHeaderValue(
            m_prevVmsApiRequest->headers, nx::network::http::header::Authorization::NAME);
        ASSERT_FALSE(authorizationHeaderStr.isEmpty());
        nx::network::http::header::Authorization authorization;
        ASSERT_TRUE(authorization.parse(authorizationHeaderStr));
        ASSERT_EQ(m_ownerAccount.email, authorization.userid().toStdString());
    }

    void assertMasterSystemInfoDoesNotContainMergeInfo()
    {
        assertSystemMergeInfoIsNotAvailable(m_masterSystem.id);
    }

    void assertMasterSystemInfoContainsMergeInfo()
    {
        assertSystemMergeInfo(m_masterSystem.id, api::MergeRole::master, m_slaveSystem.id);
    }

    void assertSlaveSystemInfoContainsMergeInfo()
    {
        assertSystemMergeInfo(m_slaveSystem.id, api::MergeRole::slave, m_masterSystem.id);
    }

    void assertAnyOfPermissionsIsNotEnoughToMergeSystems(
        std::vector<api::SystemAccessRole> accessRolesToCheck)
    {
        for (const auto accessRole: accessRolesToCheck)
        {
            auto user = addActivatedAccount2();
            shareSystemEx(m_ownerAccount, m_masterSystem, user.email, accessRole);
            shareSystemEx(m_ownerAccount, m_slaveSystem, user.email, accessRole);

            ASSERT_EQ(
                api::ResultCode::forbidden,
                mergeSystems(user, m_masterSystem.id, m_slaveSystem.id));
        }
    }

    void waitUntilSlaveSystemIsRemoved()
    {
        for (;;)
        {
            std::vector<api::SystemDataEx> systems;
            ASSERT_EQ(
                api::ResultCode::ok,
                getSystems(m_ownerAccount.email, m_ownerAccount.password, &systems));

            if (!nx::utils::contains_if(
                    systems,
                    [this](const auto& system) { return system.id == m_slaveSystem.id; }))
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    AccountWithPassword m_ownerAccount;
    api::SystemData m_masterSystem;
    api::SystemData m_slaveSystem;
    api::Result m_prevResult;
    SystemHealthInfoProviderStub* m_systemHealthInfoProviderStub = nullptr;
    SystemHealthInfoProviderFactory::Function m_factoryBak;
    nx::network::http::TestHttpServer m_vmsGatewayEmulator;
    nx::utils::SyncQueue<nx::network::http::Request> m_vmsApiRequests;
    std::optional<nx::network::http::Request> m_prevVmsApiRequest;
    std::optional<nx::network::http::StatusCode::Value> m_vmsApiResult;
    QnJsonRestResult m_vmsResponse;

    std::unique_ptr<AbstractSystemHealthInfoProvider> createSystemHealthInfoProvider(
        clusterdb::engine::ConnectionManager*,
        nx::sql::AsyncSqlQueryExecutor*)
    {
        auto systemHealthInfoProvider = std::make_unique<SystemHealthInfoProviderStub>();
        m_systemHealthInfoProviderStub = systemHealthInfoProvider.get();
        return std::move(systemHealthInfoProvider);
    }

    void vmsApiRequestStub(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        m_vmsApiRequests.push(std::move(requestContext.request));

        nx::network::http::RequestResult requestResult(
            m_vmsApiResult ? *m_vmsApiResult : nx::network::http::StatusCode::ok);
        requestResult.dataSource = std::make_unique<nx::network::http::BufferSource>(
            "application/json",
            QJson::serialized(m_vmsResponse));

        completionHandler(std::move(requestResult));
    }

    void assertSystemMergeInfo(
        const std::string& systemId,
        api::MergeRole role,
        const std::string& anotherSystemId)
    {
        api::SystemDataEx system;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystem(m_ownerAccount.email, m_ownerAccount.password, systemId, &system));
        ASSERT_TRUE(static_cast<bool>(system.mergeInfo));
        ASSERT_EQ(role, system.mergeInfo->role);
        ASSERT_EQ(anotherSystemId, system.mergeInfo->anotherSystemId);
    }

    void assertSystemMergeInfoIsNotAvailable(
        const std::string& systemId)
    {
        api::SystemDataEx system;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystem(m_ownerAccount.email, m_ownerAccount.password, systemId, &system));
        ASSERT_FALSE(static_cast<bool>(system.mergeInfo));
    }
};

TEST_F(SystemMerge, merge_request_moves_slave_system_to_beingMerged_state)
{
    givenTwoOnlineSystemsWithSameOwner();

    whenMergeSystems();

    thenMergeSucceeded();
    andSlaveSystemIsMovedToBeingMergedState();
}

TEST_F(SystemMerge, merge_information_is_added_to_the_system)
{
    givenTwoOnlineSystemsBeingMerged();

    assertMasterSystemInfoContainsMergeInfo();
    assertSlaveSystemInfoContainsMergeInfo();
}

TEST_F(SystemMerge, merge_information_is_persistent)
{
    givenTwoOnlineSystemsBeingMerged();

    ASSERT_TRUE(restart());

    assertMasterSystemInfoContainsMergeInfo();
    assertSlaveSystemInfoContainsMergeInfo();
}

TEST_F(SystemMerge, merge_information_is_removed_after_merge_completion)
{
    givenSystemAfterMerge();
    assertMasterSystemInfoDoesNotContainMergeInfo();
}

TEST_F(SystemMerge, fails_if_either_system_is_offline)
{
    givenTwoSystemsWithSameOwner();
    whenMergeSystems();
    thenResultCodeIs(api::ResultCode::mergedSystemIsOffline);
}

// TEST_F(SystemMerge, fails_if_either_system_is_older_than_3_2)

TEST_F(SystemMerge, vms_merge_request_is_authenticated_using_same_credentials_as_cloud_merge)
{
    givenTwoOnlineSystemsWithSameOwner();

    whenMergeSystems();

    thenMergeSucceeded();
    thenVmsRequestIsAuthenticatedWithOwnerCredentials();
}

TEST_F(SystemMerge, fails_if_systems_have_different_owners)
{
    givenTwoOnlineSystemsWithDifferentOwners();
    whenMergeSystems();
    thenResultCodeIs(api::ResultCode::forbidden);
}

TEST_F(SystemMerge, fails_if_initiating_user_does_not_have_permissions)
{
    const std::vector<api::SystemAccessRole> accessRolesToTest{
        api::SystemAccessRole::disabled,
        api::SystemAccessRole::custom,
        api::SystemAccessRole::liveViewer,
        api::SystemAccessRole::viewer,
        api::SystemAccessRole::advancedViewer,
        api::SystemAccessRole::localAdmin,
        api::SystemAccessRole::cloudAdmin,
        api::SystemAccessRole::maintenance};

    givenTwoOnlineSystemsWithSameOwner();
    assertAnyOfPermissionsIsNotEnoughToMergeSystems(accessRolesToTest);
}

TEST_F(SystemMerge, merge_request_invokes_merge_request_to_slave_system)
{
    givenTwoOnlineSystemsWithSameOwner();
    whenMergeSystems();
    thenVmsApiMergeRequestHasBeenIssuedToSlaveSystem();
}

TEST_F(SystemMerge, fails_if_request_to_slave_system_fails)
{
    givenTwoOnlineSystemsWithSameOwner();

    whenSlaveSystemFailsEveryRequest();
    whenMergeSystems();

    thenResultCodeIs(api::ResultCode::vmsRequestFailure);
}

TEST_F(SystemMerge, vms_response_error_text_is_forwarded)
{
    constexpr char kErrorText[] = "raz-raz-raz";

    givenVmsThatReturnsLogicalError(kErrorText);
    givenTwoOnlineSystemsWithSameOwner();

    whenMergeSystems();

    thenMergeFailed();
    andResponseErrorTextIs(kErrorText);
}

} // namespace test
} // namespace nx::cloud::db
