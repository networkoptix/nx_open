/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "system_manager.h"

#include "cdb_request_path.h"
#include "data/system_data.h"


namespace nx {
namespace cdb {
namespace cl {

SystemManager::SystemManager(cc::CloudModuleEndPointFetcher* const cloudModuleEndPointFetcher)
:
    AsyncRequestsExecutor(cloudModuleEndPointFetcher)
{
}

void SystemManager::bindSystem(
    api::SystemRegistrationData registrationData,
    std::function<void(api::ResultCode, api::SystemData)> completionHandler)
{
    executeRequest(
        SYSTEM_BIND_PATH,
        registrationData,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemData()));
}

void SystemManager::unbindSystem(
    const std::string& systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        SYSTEM_UNBIND_PATH,
        api::SystemID(systemID),
        completionHandler,
        completionHandler);
}

void SystemManager::getSystems(
    std::function<void(api::ResultCode, api::SystemDataList)> completionHandler)
{
    executeRequest(
        SYSTEM_GET_PATH,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemDataList()));
}

void SystemManager::shareSystem(
    api::SystemSharing sharingData,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        SYSTEM_SHARE_PATH,
        sharingData,
        completionHandler,
        completionHandler);
}

}   //cl
}   //cdb
}   //nx
