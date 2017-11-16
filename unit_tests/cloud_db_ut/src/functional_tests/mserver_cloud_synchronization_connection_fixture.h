#pragma once

#include <nx/cloud/cdb/test_support/transaction_connection_helper.h>

#include "ec2/cloud_vms_synchro_test_helper.h"

namespace nx {
namespace cdb {
namespace test {

/**
 * This test launches cdb and establishes transaction connection(s) to it.
 * Does not bring up whole appserver2 peer.
 */
class Ec2MserverCloudSynchronizationConnection:
    public CdbFunctionalTest
{
public:
    Ec2MserverCloudSynchronizationConnection();

    const AccountWithPassword& account() const;
    const api::SystemData& system() const;
    /**
     * Opens connection. Each has random peerId.
     */
    void openTransactionConnections(int count);
    /**
     * @return Ids of connections created.
     */
    std::vector<int> openTransactionConnectionsOfSpecifiedVersion(int count, int protoVersion);
    void waitForConnectionsToMoveToACertainState(
        const std::vector<::ec2::QnTransactionTransportBase::State>& desiredStates,
        std::chrono::milliseconds waitTimeout);
    int activeConnectionCount() const;
    void waitUntilActiveConnectionCountReaches(int count);
    void waitUntilClosedConnectionCountReaches(int count);
    void closeAllConnections();
    void useAnotherSystem();

    utils::Url cdbSynchronizationUrl() const;

    OnConnectionBecomesActiveSubscription& onConnectionBecomesActiveSubscription();
    OnConnectionFailureSubscription& onConnectionFailureSubscription();

protected:
    test::TransactionConnectionHelper& connectionHelper();

private:
    AccountWithPassword m_account;
    api::SystemData m_system;
    test::TransactionConnectionHelper m_connectionHelper;
    std::vector<int> m_connectionIds;

    void waitForAtLeastNConnectionsToMoveToACertainState(
        int count,
        const std::vector<::ec2::QnTransactionTransportBase::State>& desiredStates);
    int numberOfConnectionsInACertainState(
        const std::vector<::ec2::QnTransactionTransportBase::State>& desiredStates);
};

} // namespace test
} // namespace cdb
} // namespace nx
