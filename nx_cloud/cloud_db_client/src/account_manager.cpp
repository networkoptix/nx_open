/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "account_manager.h"


namespace nx {
namespace cdb {
namespace cl {

AccountManager::AccountManager(cc::CloudModuleEndPointFetcher* const cloudModuleEndPointFetcher)
:
    AsyncRequestsExecutor(cloudModuleEndPointFetcher)
{
}

void AccountManager::registerNewAccount(
    const api::AccountData& accountData,
    std::function<void(api::ResultCode, api::AccountActivationCode)> completionHandler)
{
    executeRequest(
        "/account/register",
        accountData,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::AccountActivationCode()));
}

void AccountManager::activateAccount(
    const api::AccountActivationCode& activationCode,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        "/account/activate",
        activationCode,
        completionHandler,
        completionHandler);
}

void AccountManager::getAccount(
    std::function<void(api::ResultCode, api::AccountData)> completionHandler)
{
    executeRequest(
        "/account/get",
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::AccountData()));
}

}   //cl
}   //cdb
}   //nx
