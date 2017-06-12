/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "account_manager.h"

#include "cdb_request_path.h"
#include <nx/utils/app_info.h>


namespace nx {
namespace cdb {
namespace client {

AccountManager::AccountManager(network::cloud::CloudModuleUrlFetcher* const cloudModuleEndPointFetcher):
    AsyncRequestsExecutor(cloudModuleEndPointFetcher)
{
}

void AccountManager::registerNewAccount(
    api::AccountRegistrationData accountData,
    std::function<void(api::ResultCode, api::AccountConfirmationCode)> completionHandler)
{
    auto errorHandler = std::bind(completionHandler, std::placeholders::_1, api::AccountConfirmationCode());
    accountData.customization = nx::utils::AppInfo::customizationName().toStdString();
    executeRequest(
        kAccountRegisterPath,
        std::move(accountData),
        std::move(completionHandler),
        std::move(errorHandler));
}

void AccountManager::activateAccount(
    api::AccountConfirmationCode activationCode,
    std::function<void(api::ResultCode, api::AccountEmail)> completionHandler)
{
    auto errorHandler = std::bind(completionHandler, std::placeholders::_1, api::AccountEmail());
    executeRequest(
        kAccountActivatePath,
        std::move(activationCode),
        std::move(completionHandler),
        std::move(errorHandler));
}

void AccountManager::getAccount(
    std::function<void(api::ResultCode, api::AccountData)> completionHandler)
{
    auto errorHandler = std::bind(completionHandler, std::placeholders::_1, api::AccountData());
    executeRequest(
        kAccountGetPath,
        std::move(completionHandler),
        std::move(errorHandler));
}

void AccountManager::updateAccount(
    api::AccountUpdateData accountData,
    std::function<void(api::ResultCode)> completionHandler)
{
    auto errorHandler = completionHandler;
    executeRequest(
        kAccountUpdatePath,
        std::move(accountData),
        std::move(completionHandler),
        std::move(errorHandler));
}

void AccountManager::resetPassword(
    api::AccountEmail accountEmail,
    std::function<void(api::ResultCode, api::AccountConfirmationCode)> completionHandler)
{
    auto errorHandler = std::bind(completionHandler, std::placeholders::_1, api::AccountConfirmationCode());
    executeRequest(
        kAccountPasswordResetPath,
        std::move(accountEmail),
        std::move(completionHandler),
        std::move(errorHandler));
}

void AccountManager::reactivateAccount(
    api::AccountEmail accountEmail,
    std::function<void(
        api::ResultCode,
        api::AccountConfirmationCode)> completionHandler)
{
    auto errorHandler = std::bind(completionHandler, std::placeholders::_1, api::AccountConfirmationCode());
    executeRequest(
        kAccountReactivatePath,
        std::move(accountEmail),
        std::move(completionHandler),
        std::move(errorHandler));
}

void AccountManager::createTemporaryCredentials(
    api::TemporaryCredentialsParams params,
    std::function<void(api::ResultCode, api::TemporaryCredentials)> completionHandler)
{
    auto errorHandler = std::bind(completionHandler, std::placeholders::_1, api::TemporaryCredentials());
    executeRequest(
        kAccountCreateTemporaryCredentialsPath,
        std::move(params),
        std::move(completionHandler),
        std::move(errorHandler));
}

}   //client
}   //cdb
}   //nx
