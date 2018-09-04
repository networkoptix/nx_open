#include "p2p_message_bus_test_base.h"

//#include <QtCore/QElapsedTimer>

#include <core/resource_access/user_access_data.h>

#include <nx/p2p/p2p_message_bus.h>
#include <core/resource_management/resource_pool.h>
#include "ec2_thread_pool.h"
#include <nx/network/system_socket.h>
#include "ec2_connection.h"
#include <transaction/transaction_message_bus_base.h>
#include <nx/p2p/p2p_connection.h>
#include <nx/p2p/p2p_serialization.h>
#include <nx_ec/dummy_handler.h>
#include <nx/utils/argument_parser.h>
#include <core/resource/media_server_resource.h>

#include <atomic>


namespace nx {
namespace p2p {
namespace test {

using namespace std::literals::chrono_literals;
static const std::chrono::seconds kMaxSyncTimeout = 15s;

//helper functions
static bool isOnline(const Appserver2Ptr& server)
{
    return server->moduleInstance()!=nullptr;
}

static QnUuid getUuid(const Appserver2Ptr& server)
{
    return server->moduleInstance()->commonModule()->moduleGUID();
}

static ec2::AbstractECConnection* getConnection(const Appserver2Ptr& server)
{
    return server->moduleInstance()->ecConnection();
}

AbstractTransactionMessageBus* getBus(const Appserver2Ptr& server)
{
    return server->moduleInstance()->ecConnection()->messageBus();
}

static QnResourcePool* getPool(const Appserver2Ptr& server)
{
    return server->moduleInstance()->commonModule()->resourcePool();
}

static QnMediaServerResourcePtr getResource(const Appserver2Ptr& server)
{
    const QnUuid& uuid = server->moduleInstance()->commonModule()->moduleGUID();
    return server->moduleInstance()->commonModule()->resourcePool()->
        getResourceById<QnMediaServerResource>(uuid);
}
//end of helper functions
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class P2pOrphanCamerasEraseTest : public P2pMessageBusTestBase
{
    QMetaObject::Connection onRemoverConnection;
    std::atomic<bool> camera3IsRemoved;
    QnUuid m_camera3Id;
public:
    //step 0
    void givenThreeSyncronizedServersAndTwoCamerasOnServer0()
    {
        startServers(3);
        setLowDelayIntervals();

        //here we change default OrphanCameraWatcher update interval to 1 second
        for (size_t i = 0; i < 3; ++i)
        {
            auto directConnection = dynamic_cast<Ec2DirectConnection*>(m_servers[i]->moduleInstance()->ecConnection());
            directConnection->orphanCameraWatcher()->changeIntervalAsync(1s);
        }

        createData(m_servers[0], 2, 0);
        createData(m_servers[1], 0, 0);
        createData(m_servers[2], 0, 0);

        circleConnect(m_servers);
        /*
                S1      S2           S1------S2
                                      \      /
                               =>      \    /
                                        \  /
                    S0 (C1+C2)           S0 (C1+C2)
        */

        //wait condition 1: connection established
        bool result1 = waitForCondition(
            [&]()
        {
            QnMediaServerResourceList servers = getPool(m_servers[0])->getAllServers(Qn::AnyStatus);
            auto distance01 = getBus(m_servers[0])->distanceToPeer(getUuid(m_servers[1]));
            auto distance02 = getBus(m_servers[0])->distanceToPeer(getUuid(m_servers[2]));
            auto distance12 = getBus(m_servers[1])->distanceToPeer(getUuid(m_servers[2]));
            auto full_distance = distance01 + distance02 + distance12;
            return servers.size() == m_servers.size() && ((full_distance == 3) || (full_distance == 4));
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result1);

        //wait condition 2: S0, S1 and S2 know about S0 cameras
        bool result2 = waitForCondition(
            [&]()
        {
            QnVirtualCameraResourceList cameraList00=getPool(m_servers[0])->getAllCameras(getResource(m_servers[0]));
            QnVirtualCameraResourceList cameraList10 = getPool(m_servers[1])->getAllCameras(getResource(m_servers[0]));
            QnVirtualCameraResourceList cameraList20 = getPool(m_servers[2])->getAllCameras(getResource(m_servers[0]));
            return cameraList00.size() == 2 && cameraList10.size() == 2 && cameraList20.size() == 2;
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result2);
    }

    //step 1
    void whenDisconnectServer2()
    {
        disconnectServers(m_servers[1], m_servers[2]);
        disconnectServers(m_servers[2], m_servers[0]);
        /*
                S1------S2           S1      S2
                 \      /             \
                  \    /       =>      \
                   \  /                 \
                    S0 (C1+C2)           S0 (C1+C2)
        */

        //wait condition 2: (S1 is disconnected from S2) and (S0 is disconnected from S2)
        bool result = waitForCondition(
            [&]()
        {
            auto distance12 = getBus(m_servers[1])->distanceToPeer(getUuid(m_servers[2]));
            auto distance02 = getBus(m_servers[0])->distanceToPeer(getUuid(m_servers[2]));
            return (distance12 > kMaxOnlineDistance) && (distance02 > kMaxOnlineDistance);
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result);
    }

    //step 2
    void whenAddCameraToServer0()
    {
        const QnUuid cam3Id(guidFromArbitraryData(QString("cam3")));

        nx::vms::api::CameraData cameraData;
        cameraData.typeId = QnResourceTypePool::instance()->getResourceTypeByName("Camera")->getId();
        cameraData.parentId = getUuid(m_servers[0]);
        cameraData.vendor = "Invalid camera";
        cameraData.physicalId = QString("cam3");
        cameraData.name = m_servers[0]->moduleInstance()->endpoint().toString();
        m_camera3Id = cameraData.id = nx::vms::api::CameraData::physicalIdToId(cameraData.physicalId); /*guidFromArbitraryData(QString("cam3"))*/
        ec2::ErrorCode err = getConnection(m_servers[0])->getCameraManager(Qn::kSystemAccess)->addCameraSync(cameraData);
        ASSERT_EQ(err, ec2::ErrorCode::ok);
        /*
                S1      S2           S1      S2
                 \                    \
                  \            =>      \
                   \                    \
                    S0 (C1+C2)           S0 (C1+C2+C3)
        */

        //wait condition 2: (S1 knows about S0's new camera) and (S0 knows about S0's new camera)
        bool result = waitForCondition(
            [&]()
        {
            QnResourcePtr resource0 = getPool(m_servers[0])->getResourceById(m_camera3Id);
            QnResourcePtr resource1 = getPool(m_servers[1])->getResourceById(m_camera3Id);
            return resource0 && resource0->getParentId() == getUuid(m_servers[0]) &&
                   resource1 && resource1->getParentId() == getUuid(m_servers[0]);
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result);
    }

    //step 3
    void whenDisconnectServer1AndServer0()
    {
        disconnectServers(m_servers[0], m_servers[1]);

        /*
                S1      S2           S1      S2
                 \
                  \           =>
                   \
                    S0 (C1+C2+C3)        S0 (C1+C2+C3)
        */

        //wait condition 2: (S1 is disconnected from S0)
        bool result = waitForCondition(
            [&]()
        {
            auto distance10 = getBus(m_servers[1])->distanceToPeer(getUuid(m_servers[0]));
            auto distance01 = getBus(m_servers[0])->distanceToPeer(getUuid(m_servers[1]));
            return distance10 > kMaxOnlineDistance && distance01 > kMaxOnlineDistance;
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result);

        /*
            postcondition: S0 knows that S0 has 3 cameras
                           S1 knows that S0 has 3 cameras
                           S2 knows that S0 has 2 cameras
       */
        QnVirtualCameraResourceList cameraList00 = getPool(m_servers[0])->getAllCameras(getResource(m_servers[0]));
        QnVirtualCameraResourceList cameraList10 = getPool(m_servers[1])->getAllCameras(getResource(m_servers[0]));
        QnVirtualCameraResourceList cameraList20 = getPool(m_servers[2])->getAllCameras(getResource(m_servers[0]));

        ASSERT_EQ(cameraList00.size(), 3);
        ASSERT_EQ(cameraList10.size(), 3);
        ASSERT_EQ(cameraList20.size(), 2);   //#########code review: sometimes cameraList[2].size()==0    ?????????
    }

    //step 4
    void whenDeleteServer0ByServer2AndStopServer0()
    {

        //server2 needs to know, that server0 is stopped, so we call removeSync
        ec2::ErrorCode err = getConnection(m_servers[2])->getMediaServerManager(Qn::kSystemAccess)->removeSync(getUuid(m_servers[0]));
        ASSERT_EQ(err, ec2::ErrorCode::ok);

        m_servers[0]->stop();

        /*
                S1      S2           S1      S2

                              =>

                    S0 (C1+C2+C3)        XX (C1+C2+C3)
        */

        //wait condition: S2 knows that S0 has no cameras
        bool result = waitForCondition(
            [&]()
        {
            //this is request to database, instead of pool? so it's wrong
            //ec2::ApiCameraDataList cameraList[3];
            //getConnection(m_servers[2])->getCameraManager(Qn::kSystemAccess)->getCamerasSync(&cameraList[2]);
            //return cameraList[2].empty();

            //this is correct request
            QnVirtualCameraResourceList cameraList20 = getPool(m_servers[2])->getAllCameras(QnResourcePtr());
            return cameraList20.empty();
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result);
    }

    void OnCameraRemoved(const QnResourcePtr &resource)
    {
        static std::list<QnUuid> removedIds;

        QnUuid id = resource->getId();

        removedIds.push_back(id);

        QnUuid parentId = resource->getParentId();
        if (id == m_camera3Id)
        {
            camera3IsRemoved = true;
        }
    }

    //step 5
    void whenConnectServer1AndServer2()
    {
        camera3IsRemoved = false;
        onRemoverConnection = QObject::connect(getPool(m_servers[1]), &QnResourcePool::resourceRemoved, [&](const QnResourcePtr &resource) {this->OnCameraRemoved(resource); });

        connectServers(m_servers[1], m_servers[2]);
        /*
                S1      S2           S1------S2

                              =>

                    XX (C1+C2+C3)        XX (C1+C2+C3)
        */

        //wait condition 1: S1 and S2 are connected
        bool result1 = waitForCondition(
            [&]()
        {
            auto distance12 = getBus(m_servers[1])->distanceToPeer(getUuid(m_servers[2]));
            auto distance21 = getBus(m_servers[2])->distanceToPeer(getUuid(m_servers[1]));
            return distance12 == 1 && distance21 == 1;
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result1);

        //wait condition 2: S1 and S2 know about each other
        bool result2 = waitForCondition(
            [&]()
        {
            QnResourcePtr resource12 = getPool(m_servers[1])->getResourceById(getUuid(m_servers[2]));
            QnResourcePtr resource21 = getPool(m_servers[2])->getResourceById(getUuid(m_servers[2]));
            return (resource12 && resource21);
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result2);
    }

    void thenNoCamerasFound()
    {

        bool result0 = waitForCondition(
            [&]()
        {

            QnVirtualCameraResourceList cameras1 = getPool(m_servers[1])->getAllCameras(QnResourcePtr());
            QnVirtualCameraResourceList cameras2 = getPool(m_servers[2])->getAllCameras(QnResourcePtr());

            //next four lines are for debug purposes only
            nx::vms::api::CameraDataList cameraList1;
            nx::vms::api::CameraDataList cameraList2;
            fromResourceListToApi(cameras1, cameraList1);
            fromResourceListToApi(cameras2, cameraList2);
            auto distance12 = getBus(m_servers[1])->distanceToPeer(getUuid(m_servers[2]));
            auto distance21 = getBus(m_servers[2])->distanceToPeer(getUuid(m_servers[1]));

            bool removed = camera3IsRemoved.load();
            return removed;
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result0);

        //std::this_thread::sleep_for(std::chrono::seconds(10));

        QObject::disconnect(onRemoverConnection);
/*
        bool result1 = waitForCondition(
            [&]()
        {
            //server1 view
            QnVirtualCameraResourceList cameras1 = m_contexts[1].pool->getAllCameras(QnResourcePtr());

            //server2 view
            QnVirtualCameraResourceList cameras2 = m_contexts[2].pool->getAllCameras(QnResourcePtr());

            //next four lines are for debug purposes only
            ApiCameraDataList cameraList1;
            ApiCameraDataList cameraList2;
            fromResourceListToApi(cameras1, cameraList1);
            fromResourceListToApi(cameras2, cameraList2);

            return cameras1.empty() && cameras2.empty();
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result1);

        std::this_thread::sleep_for(std::chrono::seconds(3));

        bool result2 = waitForCondition(
            [&]()
        {
            //server1 view
            QnVirtualCameraResourceList cameras1 = m_contexts[1].pool->getAllCameras(QnResourcePtr());

            //server2 view
            QnVirtualCameraResourceList cameras2 = m_contexts[2].pool->getAllCameras(QnResourcePtr());

            //next four lines are for debug purposes only
            ApiCameraDataList cameraList1;
            ApiCameraDataList cameraList2;
            fromResourceListToApi(cameras1, cameraList1);
            fromResourceListToApi(cameras2, cameraList2);

            return cameras1.empty() && cameras2.empty();
        }, kMaxSyncTimeout);
        ASSERT_TRUE(result2);
        */

    }

};

TEST_F(P2pOrphanCamerasEraseTest, a_server_looks_for_orphan_cameras_and_erases_them)
{
    givenThreeSyncronizedServersAndTwoCamerasOnServer0();

    whenDisconnectServer2();
    whenAddCameraToServer0(); //< 3 cameras at 0 and 1, 2 cameras at 2

    whenDisconnectServer1AndServer0();
    whenDeleteServer0ByServer2AndStopServer0();

    whenConnectServer1AndServer2();

    thenNoCamerasFound();
}

} // namespace test
} // namespace p2p
} // namespace nx
