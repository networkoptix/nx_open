#pragma once

#include <map>
#include <set>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>

#include <QtCore/QObject>

#include <gtest/gtest.h>

#include <test_support/mediaserver_launcher.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <nx/fusion/model_functions.h>

#include <nx/vms/server/event/server_runtime_event_manager.h>

// TODO: #rvasilenko investigate why receiving a `remotePeerFound` signal doesn't guarantee that
// non-persistent transaction will be received by another peer (just change the value of this macro
// to 1).
#define USE_REMOTE_PEER_FOUND_SIGNAL 0

namespace nx::vms::server::event {

static const QnUuid kDeviceId("{374b64c8-e8f2-4961-83b7-90180baa5c9b}");
static const QnUuid kEngineId("{78632931-72f9-4710-9f76-026759df2c6f}");

using namespace nx::vms::api;
using namespace std::literals::chrono_literals;

class ServerRuntimeEventManagerTest: public ::testing::Test
{
protected:
    void givenMultipleServers(int serverCount)
    {
        makeServers(serverCount);
        connectAllServers();
        waitUntilConnectionEstablished();
    }

    void afterSendingDeviceSettingsMaybeChangedEventFromServer(int serverIndex)
    {
        ASSERT_TRUE(serverIndex < m_servers.size());
        auto serverRuntimeEventManager =
            m_servers[serverIndex]->serverModule()->serverRuntimeEventManager();

        ASSERT_TRUE(serverRuntimeEventManager);

        serverRuntimeEventManager->triggerDeviceAgentSettingsMaybeChangedEvent(
            kDeviceId,
            kEngineId);
    }

    void makeSureEventHasBeenDeliveredToServer(int serverIndex)
    {
        static const std::chrono::seconds kMaxServerRuntimeEventTransactionAwaitTime(30);
        bool isEventDataCorrect = false;
        nx::utils::ElapsedTimer timer;
        timer.restart();

        do
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            auto eventData = m_eventDataByServer[serverIndex];
            isEventDataCorrect = eventData
                && eventData->deviceId == kDeviceId
                && eventData->engineId == kEngineId;

            if (!isEventDataCorrect)
                std::this_thread::sleep_for(10ms);

        } while (!isEventDataCorrect
            && timer.elapsed() < kMaxServerRuntimeEventTransactionAwaitTime);

        ASSERT_TRUE(isEventDataCorrect);
    }

private:
    void makeServers(int serverCount)
    {
        for (int i = 0; i < serverCount; ++i)
        {
            auto server = std::make_unique<MediaServerLauncher>(/*tmpDir*/ "");
            ASSERT_TRUE(server->start());

            QObject::connect(
                server->commonModule()->ec2Connection().get(),
                &ec2::AbstractECConnection::serverRuntimeEventOccurred,
                [this, i](const nx::vms::api::ServerRuntimeEventData& eventData)
                {
                    bool success = false;
                    const auto eventPayload =
                        QJson::deserialized<DeviceAgentSettingsMaybeChangedData>(
                            eventData.eventData,
                            DeviceAgentSettingsMaybeChangedData(),
                            &success);

                    NX_MUTEX_LOCKER lock(&m_mutex);
                    if (success)
                        m_eventDataByServer[i] = eventPayload;
                });

            m_servers.push_back(std::move(server));
        }
    }

    void waitUntilConnectionEstablished()
    {
        static const std::chrono::seconds kSyncWaitTimeout(30);
        bool isConnectionEstablished = false;
        nx::utils::ElapsedTimer timer;
        timer.restart();

        #if USE_REMOTE_PEER_FOUND_SIGNAL
            NX_MUTEX_LOCKER lock(&m_mutex);
            while (m_establishedConnectionCount < m_servers.size()
                && timer.elapsed() < kSyncWaitTimeout)
            {
                m_waitCondition.wait(
                    &m_mutex,
                    std::max(1ms, kSyncWaitTimeout - timer.elapsed()));
            }

            isConnectionEstablished = m_establishedConnectionCount == m_servers.size();
        #else
            do
            {
                isConnectionEstablished = std::all_of(
                    m_servers.cbegin(),
                    m_servers.cend(),
                    [this](const auto& server)
                    {
                        nx::vms::api::MediaServerDataList serverDataList;
                        auto ecConnection = server->commonModule()->ec2Connection();
                        ecConnection->getMediaServerManager(Qn::kSystemAccess)
                            ->getServersSync(&serverDataList);

                        return serverDataList.size() == m_servers.size();
                    });

                if (!isConnectionEstablished)
                    std::this_thread::sleep_for(10ms);

            } while (!isConnectionEstablished && timer.elapsed() < kSyncWaitTimeout);
        #endif

        ASSERT_TRUE(isConnectionEstablished);
    }

    void connectAllServers()
    {
        for (int i = 0; i < m_servers.size(); ++i)
        {
            #if USE_REMOTE_PEER_FOUND_SIGNAL
                QObject::connect(
                    m_servers[i]->commonModule()->ec2Connection().get(),
                    &ec2::AbstractECConnection::remotePeerFound,
                    [this](QnUuid id, nx::vms::api::PeerType /*peerType*/)
                    {
                        NX_MUTEX_LOCKER lock(&m_mutex);
                        if (m_serversThatAlreadySentSignals.find(id) ==
                            m_serversThatAlreadySentSignals.cend())
                        {
                            m_serversThatAlreadySentSignals.insert(id);
                            ++m_establishedConnectionCount;
                            m_waitCondition.wakeOne();
                        }
                    });
            #endif

            if (i < m_servers.size() - 1)
                m_servers[i]->connectTo(m_servers[i + 1].get());
        }
    }

private:
    nx::Mutex m_mutex;
    nx::WaitCondition m_waitCondition;
    int m_establishedConnectionCount = 0;
    std::set<QnUuid> m_serversThatAlreadySentSignals;

    std::map</*serverIndex*/ int, std::optional<nx::vms::api::DeviceAgentSettingsMaybeChangedData>>
        m_eventDataByServer;

    std::vector<std::unique_ptr<MediaServerLauncher>> m_servers;
};

TEST_F(ServerRuntimeEventManagerTest, serverRuntimeEventDeliveredToAnotherServer)
{
    givenMultipleServers(2);
    afterSendingDeviceSettingsMaybeChangedEventFromServer(0);
    makeSureEventHasBeenDeliveredToServer(1);
}

} // namespace nx::vms::server::event
