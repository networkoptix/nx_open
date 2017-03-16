#pragma once

#include <test_support/transaction_connection_helper.h>

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
    void openTransactionConnections(int count);
    void openTransactionConnectionsOfSpecifiedVersion(int count, int protoVersion);
    void waitForConnectionsToMoveToACertainState(
        const std::vector<::ec2::QnTransactionTransportBase::State>& desiredStates,
        std::chrono::milliseconds waitTimeout);
    int activeConnectionCount() const;
    void waitUntilActiveConnectionCountReaches(int count);
    void waitUntilClosedConnectionCountReaches(int count);
    void closeAllConnections();

    void setOnConnectionBecomesActive(
        test::TransactionConnectionHelper::ConnectionStateChangeHandler handler);
    void setOnConnectionFailure(
        test::TransactionConnectionHelper::ConnectionStateChangeHandler handler);

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
