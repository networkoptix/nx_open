#pragma once

#include <gtest/gtest.h>

#include <nx/utils/test_support/module_instance_launcher.h>

#include <common/static_common_module.h>
#include <test_support/appserver2_process.h>

#include "../test_setup.h"

namespace nx {
namespace cdb {

struct TestParams
{
    TestParams() {}
    TestParams(bool p2pMode): p2pMode(p2pMode) {}

    bool p2pMode = false;
};

class Ec2MserverCloudSynchronization:
    public ::testing::TestWithParam<TestParams>
{
public:
    Ec2MserverCloudSynchronization();
    ~Ec2MserverCloudSynchronization();

    utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic>* appserver2();
    CdbLauncher* cdb();
    const CdbLauncher* cdb() const;
    api::ResultCode setOwnerAccountCredentials(
        const std::string& email,
        const std::string& password);
    api::ResultCode bindSystemToOwnerAccount();
    api::ResultCode registerAccountAndBindSystemToIt();
    api::ResultCode unbindSystem();
    api::ResultCode rebindSystem();
    api::ResultCode fillRegisteredSystemDataByCredentials(
        const std::string& systemId,
        const std::string& authKey);
    api::ResultCode saveCloudSystemCredentials(
        const std::string& id,
        const std::string& authKey);
    const AccountWithPassword& ownerAccount() const;
    const api::SystemData& registeredSystemData() const;
    QUrl cdbEc2TransactionUrl() const;

protected:
    void establishConnectionBetweenVmsAndCloud();
    void breakConnectionBetweenVmsAndCloud();

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
    void verifyThatSystemDataMatchInCloudAndVms(
        bool assertOnFailure = true,
        bool* const result = nullptr);
    void waitForCloudAndVmsToSyncUsers(
        bool assertOnFailure = true,
        bool* const result = nullptr);
    void waitForCloudAndVmsToSyncSystemData(
        bool assertOnFailure = true,
        bool* const result = nullptr);
    api::ResultCode fetchCloudTransactionLog(
        ::ec2::ApiTransactionDataList* const transactionList);
    api::ResultCode fetchCloudTransactionLogFromMediaserver(
        ::ec2::ApiTransactionDataList* const transactionList);

private:
    QnStaticCommonModule m_staticCommonModule;
    utils::test::ModuleLauncher<::ec2::Appserver2ProcessPublic> m_appserver2;
    CdbLauncher m_cdb;
    AccountWithPassword m_account;
    api::SystemData m_system;

    api::ResultCode fetchTransactionLog(
        const QUrl& url,
        ::ec2::ApiTransactionDataList* const transactionList);
    bool findAdminUserId(QnUuid* const id);
};

} // namespace cdb
} // namespace nx
