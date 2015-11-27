/**********************************************************
* Sep 29, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_UT_TEST_SETUP_H
#define NX_CDB_UT_TEST_SETUP_H

#include <future>
#include <vector>

#include <gtest/gtest.h>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <cdb/connection.h>
#include <cloud_db_process_public.h>


namespace nx {
namespace cdb {

class CdbFunctionalTest
:
    public ::testing::Test
{
public:
    //!Calls \a start
    CdbFunctionalTest();
    ~CdbFunctionalTest();

    void start();
    void waitUntilStarted();
    void stop();
    //!restarts process
    void restart();

    SocketAddress endpoint() const;

protected:
    nx::cdb::api::ConnectionFactory* connectionFactory();
    api::ModuleInfo moduleInfo() const;

    api::ResultCode addAccount(
        api::AccountData* const accountData,
        std::string* const password,
        api::AccountActivationCode* const activationCode);
    api::ResultCode activateAccount(
        const api::AccountActivationCode& activationCode);
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

    api::ResultCode bindRandomSystem(
        const std::string& email,
        const std::string& password,
        api::SystemData* const systemData);
    api::ResultCode getSystems(
        const std::string& email,
        const std::string& password,
        std::vector<api::SystemData>* const systems);
    api::ResultCode shareSystem(
        const std::string& email,
        const std::string& password,
        const QnUuid& systemID,
        const std::string& accountEmail,
        api::SystemAccessRole accessRole);
    api::ResultCode updateSystemSharing(
        const std::string& email,
        const std::string& password,
        const QnUuid& systemID,
        const std::string& accountEmail,
        api::SystemAccessRole newAccessRole);
    api::ResultCode getSystemSharings(
        const std::string& email,
        const std::string& password,
        std::vector<api::SystemSharing>* const sharings);
    api::ResultCode getSystemSharings(
        const std::string& email,
        const std::string& password,
        const std::string& systemID,
        std::vector<api::SystemSharing>* const sharings);

    api::SystemAccessRole accountAccessRoleForSystem(
        const std::vector<api::SystemSharing>& sharings,
        const std::string& accountEmail,
        const QnUuid& systemID) const;

private:
    QString m_tmpDir;
    int m_port;
    std::vector<char*> m_args;
    std::unique_ptr<CloudDBProcessPublic> m_cdbInstance;
    std::future<int> m_cdbProcessFuture;
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
