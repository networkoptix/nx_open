#include "p2p_message_bus_test_base.h"


namespace nx::p2p::test {

using namespace std::literals::chrono_literals;
static const std::chrono::seconds kMaxSyncTimeout = 150000s;

static QnResourcePool* getPool(const Appserver2Ptr& server)
{
    return server->moduleInstance()->commonModule()->resourcePool();
}

class P2pSubscriptionTest: public P2pMessageBusTestBase
{
public:

    void givenThreeStartedServers()
    {
        static const int kServerCount = 3;
        startServers(kServerCount);
        for (int i = 0; i < kServerCount; ++i)
            createData(m_servers[i], 0, 0);
        setLowDelayIntervals();
    }

    void givenSomeDataOnServer0()
    {
        createCameras(m_servers[0], 2, 0);
    }

    void whenConnectedServers(int from, int to)
    {
        connectServers(m_servers[from], m_servers[to]);
    }

    void whenDisconnectServers(int from, int to)
    {
        disconnectServers(m_servers[from], m_servers[to]);

        bool result = waitForCondition(
            [&]()
            {

                const QnUuid fromId = m_servers[from]->moduleInstance()->commonModule()->moduleGUID();
                auto server = getPool(m_servers[to])->getResourceById(fromId);
                return server && server->getStatus() == Qn::ResourceStatus::Offline;
            }, kMaxSyncTimeout);
        ASSERT_TRUE(result);
    }

    void thenServersAreSynchronized(int from, int to)
    {
        bool result = waitForCondition(
            [&]()
        {

            QnVirtualCameraResourceList cameras1 = getPool(m_servers[from])->getAllCameras(QnResourcePtr());
            QnVirtualCameraResourceList cameras2 = getPool(m_servers[to])->getAllCameras(QnResourcePtr());

            nx::vms::api::CameraDataList cameraList1;
            nx::vms::api::CameraDataList cameraList2;
            fromResourceListToApi(cameras1, cameraList1);
            fromResourceListToApi(cameras2, cameraList2);

            return cameraList1 == cameraList2;
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result);
    }
};

TEST_F(P2pSubscriptionTest, main)
{
    givenThreeStartedServers();
    givenSomeDataOnServer0();

    whenConnectedServers(0, 1);
    thenServersAreSynchronized(0, 1);

    whenDisconnectServers(0, 1);
    whenConnectedServers(1, 2);
    thenServersAreSynchronized(1, 2);

    givenSomeDataOnServer0();
    whenDisconnectServers(1, 2);
    whenConnectedServers(0, 2);
    thenServersAreSynchronized(0, 2);
    whenDisconnectServers(0, 2);

    whenConnectedServers(1, 2);

    thenServersAreSynchronized(1, 2);
}

} // namespace nx::p2p::test
