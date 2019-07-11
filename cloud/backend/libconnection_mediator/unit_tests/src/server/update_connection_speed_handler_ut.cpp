#include "../functional_tests/mediator_functional_test.h"

#include <nx/cloud/mediator/mediator_service.h>
#include <nx/cloud/mediator/listening_peer_db.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/network/cloud/mediator/api/mediator_api_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/thread/sync_queue.h>

#include "test_connection.h"

namespace nx::hpm::test {

class UpdateConnectionSpeedHandler:
    public MediatorFunctionalTest
{
public:
    UpdateConnectionSpeedHandler()
    {
        addArg("-listeningPeerDb/enabled", "true");
        addArg("-listeningPeerDb/enableCache", "true");
        addArg("-listeningPeerDb/sql/driverName", "QSQLITE");
    }

    ~UpdateConnectionSpeedHandler()
    {
        moduleInstance()->impl()->listeningPeerDb().unsubscribeFromUplinkSpeedUpdated(
            m_uplinkSpeedUpdatedId);

        stop();
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(startAndWaitUntilStarted());

        m_mediatorApiClient = std::make_unique<hpm::api::Client>(
            network::url::Builder().setScheme(network::http::kUrlSchemeName)
                .setEndpoint(httpEndpoint()));

        m_uplinkSpeedUpdatedId =
            moduleInstance()->impl()->listeningPeerDb().subscribeToUplinkSpeedUpdated(
                [this](hpm::api::PeerConnectionSpeed uplinkSpeed)
                {
                    m_receivedUplinkSpeed.push(std::move(uplinkSpeed));
                });
    }

    void whenSendUplinkSpeedToMediator()
    {
        auto system = addRandomSystem();
        m_expectedUplinkSpeed.systemId = nx::toStdString(system.id);
        m_expectedUplinkSpeed.serverId = nx::toStdString(addRandomServer(system)->serverId());
        m_expectedUplinkSpeed.connectionSpeed.bandwidth = nx::utils::random::number(10, 10000);
        m_expectedUplinkSpeed.connectionSpeed.pingTime =
            std::chrono::microseconds(nx::utils::random::number(10, 10000));

        m_mediatorApiClient->reportUplinkSpeed(
            m_expectedUplinkSpeed,
            [](hpm::api::ResultCode resultCode)
            {
                ASSERT_EQ(hpm::api::ResultCode::ok, resultCode);
            });
    }

    void thenMediatorReceivesUplinkSpeed()
    {
        ASSERT_EQ(m_expectedUplinkSpeed, m_receivedUplinkSpeed.pop());
    }

private:
    std::unique_ptr<hpm::api::Client> m_mediatorApiClient;
    hpm::api::PeerConnectionSpeed m_expectedUplinkSpeed;
    nx::utils::SubscriptionId m_uplinkSpeedUpdatedId;
    nx::utils::SyncQueue<hpm::api::PeerConnectionSpeed> m_receivedUplinkSpeed;

};

TEST_F(UpdateConnectionSpeedHandler, actually_updates_connection_speed)
{
    whenSendUplinkSpeedToMediator();
    thenMediatorReceivesUplinkSpeed();
}

} // namespace nx::hpm::test
