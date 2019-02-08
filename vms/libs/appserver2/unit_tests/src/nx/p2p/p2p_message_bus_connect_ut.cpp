#include "p2p_message_bus_test_base.h"

#include <QtCore/QElapsedTimer>

#include <nx/vms/api/data/camera_data.h>
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
#include <core/resource/camera_resource.h>
#include <core/resource_management/status_dictionary.h>
#include <nx/p2p/p2p_server_message_bus.h>

namespace nx {
namespace p2p {
namespace test {

static const int kDefaultInstanceCount = 3;
static const int kDefaultPropertiesPerCamera = 5;
static const int kDefaultCamerasCount = 20;
static const int kDefaultUsersCount = 0;

static const char kServerCountParamName[] = "serverCount";
static const char kCameraCountParamName[] = "cameraCount";
static const char kPropertyCountParamName[] = "propertyCount";
static const char kUserCountParamName[] = "userCount";
static const char kServerPortParamName[] = "tcpPort";
static const char kStandaloneModeParamName[] = "standaloneMode";
static const char kTransactionsPerServerInSecond[] = "transactionsPerServerInSecond";

int getIntParam(const nx::utils::ArgumentParser& args, const QString& name, int defaultValue = 0)
{
    auto result = args.get<QString>(name);
    return result ? result->toInt() : defaultValue;
}

class P2pMessageBusTest: public QObject, public P2pMessageBusTestBase
{
protected:

    void printReport()
    {
        ec2::Ec2DirectConnection* connection =
            dynamic_cast<ec2::Ec2DirectConnection*> (m_servers[0]->moduleInstance()->ecConnection());
        ec2::detail::QnDbManager* db = connection->queryProcessor()->getDb();
        ec2::ApiTransactionDataList tranList;
        db->doQuery(ec2::ApiTranLogFilter(), tranList);
        qint64 totalDbData = 0;
        for (const auto& tran: tranList)
            totalDbData += tran.dataSize;

        NX_INFO(this, lit("Total bytes sent: %1, dbSize: %2, ratio: %3")
            .arg(nx::network::totalSocketBytesSent())
            .arg(totalDbData)
            .arg(nx::network::totalSocketBytesSent() / (float)totalDbData));

        const auto& counters = Connection::sendCounters();
        qint64 webSocketBytes = 0;
        for (int i = 0; i < (int) MessageType::counter; ++i)
            webSocketBytes += counters[i];

        NX_INFO(this, lit("Total bytes via P2P: %1, dbSize: %2, ratio: %3")
            .arg(webSocketBytes)
            .arg(totalDbData)
            .arg(webSocketBytes / (float)totalDbData));

        for (int i = 0; i < (int) MessageType::counter; ++i)
        {
            NX_INFO(this, lit("P2P message: %1, bytes %2, dbSize: %3, ratio: %4")
                .arg(toString(MessageType(i)))
                .arg(counters[i])
                .arg(totalDbData)
                .arg(counters[i] / (float)totalDbData));
        }

        int totalConnections = 0;
        int startedConnections = 0;
        int connectionTries = 0;
        for (const auto& server: m_servers)
        {
            ec2::Ec2DirectConnection* connection =
                dynamic_cast<ec2::Ec2DirectConnection*> (server->moduleInstance()->ecConnection());
            const auto& bus = connection->messageBus()->dynamicCast<MessageBus*>();
            for (const auto& connection: bus->connections())
            {
                ++totalConnections;
                if (bus->context(connection)->isLocalStarted)
                    ++startedConnections;
            }
            connectionTries += bus->connectionTries();
        }

        float k = (m_servers.size() - 1) * m_servers.size();
        NX_INFO(
            this,
            lit("Total connections: %1, ratio %2.  Opened %3, ratio %4.  Tries: %5, ratio %6")
            .arg(totalConnections)
            .arg(totalConnections / k)
            .arg(startedConnections)
            .arg(startedConnections / k)
            .arg(connectionTries)
            .arg(connectionTries / k));
    }

    void waitForLazyDataCommitDone()
    {
        std::vector<int> waitFlags;
        waitFlags.resize(m_servers.size());
        QnWaitCondition waitCondition;
        QnMutex mutex;
        for (int i = 0 ; i < m_servers.size(); ++i)
        {
            auto& server = m_servers[i];
            auto connection = server->moduleInstance()->ecConnection();
            const auto& bus = connection->messageBus()->dynamicCast<ServerMessageBus*>();
            connect(
                bus, &ServerMessageBus::lazyDataCommtDone,
                this,
                [i, &waitFlags, &mutex, &waitCondition]()
                {
                    QnMutexLocker lock(&mutex);
                    ++waitFlags[i];
                    waitCondition.wakeOne();
                }, Qt::DirectConnection);
        }
        QnMutexLocker lock(&mutex);
        while (!std::any_of(waitFlags.begin(), waitFlags.end(),
            [&](const int value)
            {
                return value >= 2;
            }))
        {
            waitCondition.wait(&mutex);
        }

        for (auto& server: m_servers)
        {
            auto connection = server->moduleInstance()->ecConnection();
            const auto& bus = connection->messageBus()->dynamicCast<ServerMessageBus*>();
            bus->disconnect(this);
        }
    }

    void addRuntimeData(const Appserver2Ptr& server)
    {
        auto commonModule = server->moduleInstance()->commonModule();
        auto connection = server->moduleInstance()->ecConnection();
        nx::vms::api::RuntimeData runtimeData;
        runtimeData.peer.id = commonModule->moduleGUID();
        runtimeData.peer.instanceId = commonModule->runningInstanceGUID();
        runtimeData.peer.persistentId = commonModule->dbId();
        runtimeData.peer.peerType = nx::vms::api::PeerType::server;

        connection->getMiscManager(Qn::kSystemAccess)
            ->saveRuntimeInfo(
                runtimeData,
                ec2::DummyHandler::instance(),
                &ec2::DummyHandler::onRequestDone);
    }

    void testMain(std::function<void(std::vector<Appserver2Ptr>&)> serverConnectFunc, int keepDbAtServerIndex = -1)
    {
        nx::utils::ArgumentParser args(QCoreApplication::instance()->arguments());

        m_servers.clear();
        ec2::Appserver2Process::resetInstanceCounter();

        const int instanceCount = getIntParam(args, kServerCountParamName, kDefaultInstanceCount);
        const int serverPort = getIntParam(args, kServerPortParamName);
        auto transactionsPerServerInSecond = args.get<QString>(kTransactionsPerServerInSecond);
        startServers(instanceCount, keepDbAtServerIndex, serverPort);

        QElapsedTimer t;
        t.restart();
        const int cameraCount = getIntParam(args, kCameraCountParamName, kDefaultCamerasCount);
        for (const auto& server: m_servers)
        {
            createData(
                server,
                cameraCount,
                getIntParam(args, kPropertyCountParamName, kDefaultPropertiesPerCamera),
                getIntParam(args, kUserCountParamName, kDefaultUsersCount));
            addRuntimeData(server);
        }
        NX_INFO(this, lit("Create test data time: %1 ms").arg(t.elapsed()));

        serverConnectFunc(m_servers);

        waitForSync(keepDbAtServerIndex >= 0 ? cameraCount * 2 : cameraCount);

        // Ensure all data are committed and server ready to process new requests quickly before set SF_P2pSyncDone flag.
        if (args.get<QString>(kStandaloneModeParamName))
            waitForLazyDataCommitDone();

        printReport();

        for (auto& server: m_servers)
        {
            auto commonModule = server->moduleInstance()->commonModule();
            auto serverRes = commonModule->resourcePool()->
                getResourceById<QnMediaServerResource>(commonModule->moduleGUID());
            auto flags = serverRes->getServerFlags();
            flags |= nx::vms::api::SF_P2pSyncDone;
            serverRes->setServerFlags(flags);
            commonModule->bindModuleInformation(serverRes);
        }

        using namespace std::chrono;
        std::chrono::microseconds sleepInterval = 1s;
        const bool generateTransactions = transactionsPerServerInSecond != boost::none
            && *transactionsPerServerInSecond > 0;
        if (generateTransactions)
            sleepInterval = microseconds(int(1000000.0 / transactionsPerServerInSecond->toFloat()));

        auto cameras = m_servers[0]->moduleInstance()
            ->commonModule()->resourcePool()->getAllCameras();
        int cameraIndex = 0;

        while (args.get<QString>(kStandaloneModeParamName))
        {
            if (!cameras.isEmpty() && generateTransactions)
            {

                for (auto& server: m_servers)
                {
                    auto connection = server->moduleInstance()->commonModule()->ec2Connection();
                    auto resourceManager = connection->getResourceManager(Qn::kSystemAccess);

                    auto resourceId = cameras[cameraIndex]->getId();
                    cameraIndex = (cameraIndex + 1) % cameras.size();
                    auto statusDict = server->moduleInstance()->commonModule()->statusDictionary();
                    auto oldStatus = statusDict->value(resourceId);
                    auto status = oldStatus == Qn::Offline ? Qn::Online : Qn::Offline;

                    statusDict->setValue(resourceId, status);
                    resourceManager->setResourceStatus(resourceId, status,
                        ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
                }
            }
            std::this_thread::sleep_for(sleepInterval);
        }
    }
};

TEST_F(P2pMessageBusTest, SequenceConnect)
{
    testMain(sequenceConnect);
}

TEST_F(P2pMessageBusTest, FullConnect)
{
    testMain(fullConnect);
}

TEST_F(P2pMessageBusTest, EmptyConnect)
{
    nx::utils::ArgumentParser args(QCoreApplication::instance()->arguments());
    if (args.get<QString>(kStandaloneModeParamName))
        testMain(emptyConnect);
}

TEST_F(P2pMessageBusTest, RestartServer)
{
    testMain(fullConnect);
    testMain(fullConnect, /*keepDbAtServerIndex*/ 0);
}

} // namespace test
} // namespace p2p
} // namespace nx
