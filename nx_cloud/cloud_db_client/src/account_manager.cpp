/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "account_manager.h"

#include "cdb_request_path.h"
#include <utils/common/app_info.h>


namespace nx {
namespace cdb {
namespace cl {

AccountManager::AccountManager(network::cloud::CloudModuleEndPointFetcher* const cloudModuleEndPointFetcher)
:
    AsyncRequestsExecutor(cloudModuleEndPointFetcher)
{
}

void AccountManager::registerNewAccount(
    api::AccountData accountData,
    std::function<void(api::ResultCode, api::AccountConfirmationCode)> completionHandler)
{
    accountData.customization = QnAppInfo::customizationName().toStdString();
    executeRequest(
        kAccountRegisterPath,
        std::move(accountData),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::AccountConfirmationCode()));
}

void AccountManager::activateAccount(
    api::AccountConfirmationCode activationCode,
    std::function<void(api::ResultCode, api::AccountEmail)> completionHandler)
{
    executeRequest(
        kAccountActivatePath,
        std::move(activationCode),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::AccountEmail()));
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

void AccountManager::reactivateAccount(
    api::AccountEmail accountEmail,
    std::function<void(
        api::ResultCode,
        api::AccountConfirmationCode)> completionHandler)
{
    executeRequest(
        kAccountReactivatePath,
        std::move(accountEmail),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::AccountConfirmationCode()));
}

}   //cl
}   //cdb
}   //nx
