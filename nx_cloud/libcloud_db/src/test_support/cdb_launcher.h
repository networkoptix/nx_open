#pragma once

#include <chrono>
#include <set>
#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/network/socket_common.h>
#include <nx/utils/std/future.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <utils/db/types.h>

#include <cdb/connection.h>

#include "cloud_db_process_public.h"
#include "managers/email_manager.h"
#include "test_with_db_helper.h"

namespace nx {
namespace cdb {

class AccountWithPassword:
    public api::AccountData
{
public:
    std::string password;
};

class CdbLauncher:
    public utils::test::ModuleLauncher<CloudDBProcessPublic>,
    public TestWithDbHelper
{
public:
    //!Calls \a start
    CdbLauncher(QString tmpDir = QString());
    ~CdbLauncher();

    virtual bool waitUntilStarted() override;

    SocketAddress endpoint() const;

    nx::cdb::api::ConnectionFactory* connectionFactory();
    std::unique_ptr<nx::cdb::api::Connection> connection(
        const std::string& login,
        const std::string& password);
    api::ModuleInfo moduleInfo() const;

    api::ResultCode addAccount(
        api::AccountData* const accountData,
        std::string* const password,
        api::AccountConfirmationCode* const activationCode);
    std::string generateRandomEmailAddress() const;
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
    api::ResultCode bindRandomNotActivatedSystem(
        const std::string& email,
        const std::string& password,
        const std::string& opaque,
        api::SystemData* const systemData);
    api::ResultCode bindRandomSystem(
        const std::string& email,
        const std::string& password,
        api::SystemData* const systemData);
    api::ResultCode bindRandomSystem(
        const std::string& email,
        const std::string& password,
        const std::string& opaque,
        api::SystemData* const systemData);
    api::ResultCode unbindSystem(
        const std::string& login,
        const std::string& password,
        const std::string& systemId);
    api::ResultCode getSystems(
        const std::string& email,
        const std::string& password,
        std::vector<api::SystemDataEx>* const systems);
    api::ResultCode getSystem(
        const std::string& email,
        const std::string& password,
        const std::string& systemId,
        std::vector<api::SystemDataEx>* const systems);
    api::ResultCode getSystem(
        const std::string& email,
        const std::string& password,
        const std::string& systemId,
        api::SystemDataEx* const system);

    api::ResultCode shareSystem(
        const std::string& email,
        const std::string& password,
        const api::SystemSharing& sharingData);
    api::ResultCode shareSystem(
        const std::string& email,
        const std::string& password,
        const std::string& systemId,
        const std::string& accountEmail,
        api::SystemAccessRole accessRole);
    api::ResultCode shareSystem(
        const AccountWithPassword& grantor,
        const std::string& systemId,
        const std::string& accountEmail,
        api::SystemAccessRole accessRole);

    api::ResultCode updateSystemSharing(
        const std::string& email,
        const std::string& password,
        const std::string& systemId,
        const std::string& accountEmail,
        api::SystemAccessRole newAccessRole);
    api::ResultCode removeSystemSharing(
        const std::string& email,
        const std::string& password,
        const std::string& systemId,
        const std::string& accountEmail);
    api::ResultCode getSystemSharings(
        const std::string& email,
        const std::string& password,
        std::vector<api::SystemSharingEx>* const sharings);
    api::ResultCode getSystemSharings(
        const std::string& email,
        const std::string& password,
        const std::string& systemId,
        std::vector<api::SystemSharingEx>* const sharings);
    api::ResultCode getAccessRoleList(
        const std::string& email,
        const std::string& password,
        const std::string& systemId,
        std::set<api::SystemAccessRole>* const accessRoles);
    api::ResultCode renameSystem(
        const std::string& login,
        const std::string& password,
        const std::string& systemId,
        const std::string& newSystemName);
    api::ResultCode updateSystem(
        const api::SystemData& system,
        const api::SystemAttributesUpdate& updatedData);

    //calls on system's regard
    api::ResultCode getCdbNonce(
        const std::string& systemId,
        const std::string& authKey,
        api::NonceData* const nonceData);
    api::ResultCode getCdbNonce(
        const std::string& accountEmail,
        const std::string& accountPassword,
        const std::string& systemId,
        api::NonceData* const nonceData);
    //calls on system's regard
    api::ResultCode ping(
        const std::string& systemId,
        const std::string& authKey);

    /** finds sharing of \a systemId to account \a accountEmail.
        \return reference to an element of \a sharings
    */
    const api::SystemSharingEx& findSharing(
        const std::vector<api::SystemSharingEx>& sharings,
        const std::string& accountEmail,
        const std::string& systemId) const;
    api::SystemAccessRole accountAccessRoleForSystem(
        const std::vector<api::SystemSharingEx>& sharings,
        const std::string& accountEmail,
        const std::string& systemId) const;
    api::ResultCode fetchSystemData(
        const std::string& accountEmail,
        const std::string& accountPassword,
        const std::string& systemId,
        api::SystemDataEx* const systemData);

    api::ResultCode recordUserSessionStart(
        const AccountWithPassword& account,
        const std::string& systemId);

    api::ResultCode getVmsConnections(
        api::VmsConnectionDataList* const vmsConnections);

    bool isStartedWithExternalDb() const;
    bool placePreparedDB(const QString& dbDumpPath);

private:
    int m_port;
    std::unique_ptr<
        nx::cdb::api::ConnectionFactory,
        decltype(&destroyConnectionFactory)
    > m_connectionFactory;
    api::ModuleInfo m_moduleInfo;
};

namespace api {

bool operator==(const api::AccountData& left, const api::AccountData& right);

} // namespace api

class EmailManagerStub:
    public nx::cdb::AbstractEmailManager
{
public:
    EmailManagerStub(nx::cdb::AbstractEmailManager* const target);

    virtual void sendAsync(
        const AbstractNotification& notification,
        std::function<void(bool)> completionHandler) override;

private:
    nx::cdb::AbstractEmailManager* const m_target;
};

} // namespace cdb
} // namespace nx
