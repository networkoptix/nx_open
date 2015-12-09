/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "account_manager.h"

#include "cdb_request_path.h"
#include "version.h"


namespace nx {
namespace cdb {
namespace cl {

AccountManager::AccountManager(cc::CloudModuleEndPointFetcher* const cloudModuleEndPointFetcher)
:
    AsyncRequestsExecutor(cloudModuleEndPointFetcher)
{
}

void AccountManager::registerNewAccount(
    api::AccountData accountData,
    std::function<void(api::ResultCode, api::AccountConfirmationCode)> completionHandler)
{
    accountData.customization = QN_CUSTOMIZATION_NAME;
    executeRequest(
        kAccountRegisterPath,
        std::move(accountData),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::AccountConfirmationCode()));
}

void AccountManager::activateAccount(
    api::AccountConfirmationCode activationCode,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        kAccountActivatePath,
        std::move(activationCode),
        completionHandler,
        completionHandler);
}

void AccountManager::getAccount(
    std::function<void(api::ResultCode, api::AccountData)> completionHandler)
{
    executeRequest(
        kAccountGetPath,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::AccountData()));
}

void AccountManager::updateAccount(
    api::AccountUpdateData accountData,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        kAccountUpdatePath,
        std::move(accountData),
        completionHandler,
        completionHandler);
}

void AccountManager::resetPassword(
    api::AccountEmail accountEmail,
    std::function<void(api::ResultCode, api::AccountConfirmationCode)> completionHandler)
{
    executeRequest(
        kAccountPasswordResetPath,
        std::move(accountEmail),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::AccountConfirmationCode()));
}

}   //cl
}   //cdb
}   //nx
