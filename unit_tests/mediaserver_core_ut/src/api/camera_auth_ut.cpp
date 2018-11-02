#include <memory>
#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/vms/api/data/resource_data.h>
#include <rest/server/json_rest_result.h>
#include <server_query_processor.h>

#include "test_api_requests.h"

namespace nx::test {

class CameraAuthUt: public ::testing::Test
{
protected:
private:
};

class CameraAuth: public ::testing::Test
{
protected:
    CameraAuth()
    {
        m_server = std::unique_ptr<MediaServerLauncher>(new MediaServerLauncher());
        m_server->addSetting(QnServer::kNoInitStoragesOnStartup, "1");
    }

    void whenServerLaunched()
    {
        ASSERT_TRUE(m_server->start());
    }

private:
    std::unique_ptr<MediaServerLauncher> m_server;
};

TEST_F(CameraAuthUt, amendTransaction_ParamData)
{
    // const auto originalTran = createTranWithTestParamData();
    // const auto amendedTran = whenTransactionProcessed(originalTran);
    // thenItShouldBeAmended(originalTran, amendedTran);
}

TEST_F(CameraAuthUt, amendTransaction_ParamWithRefData)
{
    // const auto originalTran = createTranWithTestParamWithRefData();
    // const auto amendedTran = whenTransactionProcessed(originalTran);
    // thenItShouldBeAmended(originalTran, amendedTran);
}

TEST_F(CameraAuth, SetGetViaNetworkResource)
{
    // whenServerLaunched();
    // whenCameraSaved();
    // whenAuthSetViaResource();
    // whenAuthRequestedViaResource();
    // thenItShouldBeOk();
}

TEST_F(CameraAuth, SetViaAPIGetViaNetworkResource)
{
}

TEST_F(CameraAuth, SetGetViaAPI)
{
}

} // namespace nx::test
