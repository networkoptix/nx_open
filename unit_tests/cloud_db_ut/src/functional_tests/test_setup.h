/**********************************************************
* Sep 29, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_UT_TEST_SETUP_H
#define NX_CDB_UT_TEST_SETUP_H

#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/utils/std/future.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <utils/db/types.h>

#include <cdb/connection.h>
#include <cloud_db_process_public.h>


namespace nx {
namespace cdb {

class CdbFunctionalTest
:
    public utils::test::ModuleLauncher<CloudDBProcessPublic>,
    public ::testing::Test
{
public:
    //!Calls \a start
    CdbFunctionalTest();
    ~CdbFunctionalTest();

    virtual bool waitUntilStarted() override;

    SocketAddress endpoint() const;

    nx::cdb::api::ConnectionFactory* connectionFactory();
    api::ModuleInfo moduleInfo() const;

    api::ResultCode addAccount(
        api::AccountData* const accountData,
        std::string* const password,
        api::AccountConfirmationCode* const activationCode);
    api::ResultCode activateAccount(
        const api::AccountConfirmationCode& activationCode,
        std::string* const accountEmail);
    api::ResultCode reactivateAccount(
        const std::string& email,
        api::AccountConfirmationCode* const activationCode);
    api::ResultCode getAccount(
        const std::string& email,
        const std::string& password,
        api::AccountData* const accountData);
    api::ResultCode addActivatedAccount(
        api::AccountData* const accountData,
        std::string* const password);
    api::ResultCode updateAccount(
        const std::string& email,
        const std::string& password,
        api::AccountUpdateData updateData);
    api::ResultCode resetAccountPassword(
        const std::string& email,
        std::string* const confirmationCode);
    api::ResultCode createTemporaryCredentials(
        const std::string& email,
        const std::string& password,
        const api::TemporaryCredentialsParams& params,
        api::TemporaryCredentials* const temporaryCredentials);

    api::ResultCode bindRandomNotActivatedSystem(
        const std::string& email,
        const std::string& password,
        api::SystemData* const systemData);
    api::ResultCode bindRandomSystem(
        const std::string& email,
        const std::string& password,
        api::SystemData* const systemData);
    api::ResultCode unbindSystem(
        const std::string& login,
        const std::string& password,
        const std::string& systemID);
    api::ResultCode getSystems(
        const std::string& email,
        const std::string& password,
        std::vector<api::SystemDataEx>* const systems);
    api::ResultCode getSystem(
        const std::string& email,
        const std::string& password,
        const std::string& systemID,
        std::vector<api::SystemDataEx>* const systems);
    api::ResultCode shareSystem(
        const std::string& email,
        const std::string& password,
        const std::string& systemID,
        const std::string& accountEmail,
        api::SystemAccessRole accessRole);
    api::ResultCode updateSystemSharing(
        const std::string& email,
        const std::string& password,
        const std::string& systemID,
        const std::string& accountEmail,
        api::SystemAccessRole newAccessRole);
    api::ResultCode getSystemSharings(
        const std::string& email,
        const std::string& password,
        std::vector<api::SystemSharingEx>* const sharings);
    api::ResultCode getSystemSharings(
        const std::string& email,
        const std::string& password,
        const std::string& systemID,
        std::vector<api::SystemSharingEx>* const sharings);
    api::ResultCode getAccessRoleList(
        const std::string& email,
        const std::string& password,
        const std::string& systemID,
        std::set<api::SystemAccessRole>* const accessRoles);
    api::ResultCode updateSystemName(
        const std::string& login,
        const std::string& password,
        const std::string& systemID,
        const std::string& newSystemName);

    //calls on system's regard
    api::ResultCode getCdbNonce(
        const std::string& systemID,
        const std::string& authKey,
        api::NonceData* const nonceData);
    //calls on system's regard
    api::ResultCode ping(
        const std::string& systemID,
        const std::string& authKey);

    /** finds sharing of \a systemID to account \a accountEmail.
        \return reference to an element of \a sharings
    */
    const api::SystemSharingEx& findSharing(
        const std::vector<api::SystemSharingEx>& sharings,
        const std::string& accountEmail,
        const std::string& systemID) const;
    api::SystemAccessRole accountAccessRoleForSystem(
        const std::vector<api::SystemSharingEx>& sharings,
        const std::string& accountEmail,
        const std::string& systemID) const;
    api::ResultCode fetchSystemData(
        const std::string& accountEmail,
        const std::string& accountPassword,
        const std::string& systemId,
        api::SystemDataEx* const systemData);

    static void setTemporaryDirectoryPath(const QString& path);
    static void setDbConnectionOptions(
        const nx::db::ConnectionOptions& connectionOptions);

private:
    QString m_tmpDir;
    int m_port;
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)
    > m_connectionFactory;
    api::ModuleInfo m_moduleInfo;
};

namespace api {
    bool operator==(const api::AccountData& left, const api::AccountData& right);
}

}   //cdb
}   //nx

#endif  //NX_CDB_UT_TEST_SETUP_H
