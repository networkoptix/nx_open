#pragma once

#include <gtest/gtest.h>

#include <nx/utils/test_support/module_instance_launcher.h>

#include "appserver2_process.h"
#include "../test_setup.h"


namespace nx {
namespace cdb {

class Ec2MserverCloudSynchronization2
    :
    public ::testing::Test
{
public:
    Ec2MserverCloudSynchronization2();
    ~Ec2MserverCloudSynchronization2();

    utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic>* appserver2();
    CdbLauncher* cdb();
    const CdbLauncher* cdb() const;
    api::ResultCode bindRandomSystem();
    const api::AccountData& ownerAccount() const;
    const std::string& ownerAccountPassword() const;
    const api::SystemData& registeredSystemData() const;
    QUrl cdbEc2TransactionUrl() const;

protected:
    void verifyTransactionConnection();
    void testSynchronizingCloudOwner();
    void testSynchronizingUserFromCloudToMediaServer();
    void testSynchronizingUserFromMediaServerToCloud();
    void addCloudUserLocally(
        const std::string& accountEmail,
        ::ec2::ApiUserData* const accountVmsData);
    void waitForUserToAppearInCloud(
        const ::ec2::ApiUserData& accountVmsData);
    void waitForUserToDisappearFromCloud(const std::string& email);
    void waitForUserToDisappearLocally(const QnUuid& userId);
    /**
     * @param found[out] Set to \a true if user has been found and verified. \a false otherwise
     */
    void verifyCloudUserPresenceInLocalDb(
        const api::SystemSharingEx& sharingData,
        bool* const found = nullptr,
        bool assertOnUserAbsense = true);
    void fetchOwnerSharing(api::SystemSharingEx* const ownerSharing);
    void verifyThatUsersMatchInCloudAndVms(
        bool assertOnFailure = true,
        bool* const result = nullptr);
    void waitForCloudAndVmsToSyncUsers(
        bool assertOnFailure = true,
        bool* const result = nullptr);
    api::ResultCode fetchCloudTransactionLog(
        ::ec2::ApiTransactionDataList* const transactionList);

private:
    utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic> m_appserver2;
    CdbLauncher m_cdb;
    api::AccountData m_account;
    std::string m_accountPassword;
    api::SystemData m_system;
};

} // namespace cdb
} // namespace nx
