/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "system_manager.h"

#include "data/system_data.h"


namespace nx {
namespace cdb {
namespace cl {

SystemManager::SystemManager(CloudModuleEndPointFetcher* const cloudModuleEndPointFetcher)
:
    AsyncRequestsExecutor(cloudModuleEndPointFetcher)
{
}

void SystemManager::bindSystem(
    api::SystemRegistrationData registrationData,
    std::function<void(api::ResultCode, api::SystemData)> completionHandler)
{
    executeRequest(
        "/system/bind",
        registrationData,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemData()));
}

void SystemManager::unbindSystem(
    const std::string& systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        "/system/unbind",
        api::SystemID(systemID),
        completionHandler,
        completionHandler);
}

void SystemManager::getSystems(
    std::function<void(api::ResultCode, api::SystemDataList)> completionHandler)
{
    executeRequest(
        "/system/get",
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemDataList()));
}

void SystemManager::shareSystem(
    api::SystemSharing sharingData,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        "/system/share",
        sharingData,
        completionHandler,
        completionHandler);
}

}   //cl
}   //cdb
}   //nx
