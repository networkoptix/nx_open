#pragma once

#include <gtest/gtest.h>

#include <vector>
#include <memory>

#include "ec2_connection.h"

#include <nx_ec/ec_api.h>
#include <common/static_common_module.h>
#include <test_support/appserver2_process.h>

#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/vms/api/data_fwd.h>

namespace nx {
namespace p2p {
namespace test {

/**
 * Base helper class for p2p message bus tests
 */
class P2pMessageBusTestBase: public testing::Test
{
public:
protected:
    static void connectServers(const Appserver2Ptr& srcServer, const Appserver2Ptr& dstServer);
    static void bidirectConnectServers(const Appserver2Ptr& srcServer, const Appserver2Ptr& dstServer);
    static void disconnectServers(const Appserver2Ptr& srcServer, const Appserver2Ptr& dstServer);

    static void sequenceConnect(std::vector<Appserver2Ptr>& servers);
    static void circleConnect(std::vector<Appserver2Ptr>& servers);
    static void fullConnect(std::vector<Appserver2Ptr>& servers);
    static void emptyConnect(std::vector<Appserver2Ptr>& servers);

    void startServers(
        int count,
        int keepDbAtServerIndex = -1,
        quint16 baseTcpPort = 0);

    void createData(
        const Appserver2Ptr& server,
        int camerasCount,
        int propertiesPerCamera,
        int userCount = 0);

    bool waitForCondition(std::function<bool()> condition, std::chrono::milliseconds timeout);
    bool waitForConditionOnAllServers(
        std::function<bool(const Appserver2Ptr&)> condition,
        std::chrono::milliseconds timeout);

    void waitForSync(int cameraCount);

    void checkMessageBus(
        std::function<bool(MessageBus*, const vms::api::PersistentIdData&)> checkFunction,
        const QString& errorMessage);
    void checkMessageBusInternal(
        std::function<bool(MessageBus*, const vms::api::PersistentIdData&)> checkFunction,
        const QString& errorMessage,
        bool waitForSync,
        int* outSyncDoneCounter);

    static bool checkSubscription(const MessageBus* bus, const vms::api::PersistentIdData& peer);
    static bool checkDistance(const MessageBus* bus, const vms::api::PersistentIdData& peer);
    bool checkRuntimeInfo(const MessageBus* bus, const vms::api::PersistentIdData& /*peer*/);

    void setLowDelayIntervals();
protected:
    std::vector<Appserver2Ptr> m_servers;
};

} // namespace test
} // namespace p2p
} // namespace nx
