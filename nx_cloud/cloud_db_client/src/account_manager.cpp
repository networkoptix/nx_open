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
    std::function<void(api::ResultCode, api::AccountActivationCode)> completionHandler)
{
    accountData.customization = QN_CUSTOMIZATION_NAME;
    executeRequest(
        ACCOUNT_REGISTER_PATH,
        std::move(accountData),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::AccountActivationCode()));
}

void AccountManager::activateAccount(
    api::AccountActivationCode activationCode,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        ACCOUNT_ACTIVATE_PATH,
        std::move(activationCode),
        completionHandler,
        completionHandler);
}

void AccountManager::getAccount(
    std::function<void(api::ResultCode, api::AccountData)> completionHandler)
{
    executeRequest(
        ACCOUNT_GET_PATH,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::AccountData()));
}

void AccountManager::updateAccount(
    api::AccountUpdateData accountData,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        ACCOUNT_UPDATE_PATH,
        std::move(accountData),
        completionHandler,
        completionHandler);
}

}   //cl
}   //cdb
}   //nx
