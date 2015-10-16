/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "system_manager.h"

#include "cdb_request_path.h"
#include "data/system_data.h"
#include "version.h"


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
    registrationData.customization = QN_CUSTOMIZATION_NAME;
    executeRequest(
        SYSTEM_BIND_PATH,
        std::move(registrationData),
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

void SystemManager::getCloudUsersOfSystem(
    std::function<void(api::ResultCode, api::SystemSharingList)> completionHandler)
{
    executeRequest(
        SYSTEM_GET_CLOUD_USERS_PATH,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemSharingList()));
}

void SystemManager::getCloudUsersOfSystem(
    const std::string& systemID,
    std::function<void(api::ResultCode, api::SystemSharingList)> completionHandler)
{
    executeRequest(
        SYSTEM_GET_CLOUD_USERS_PATH,
        api::SystemID(systemID),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemSharingList()));
}

void SystemManager::updateSharing(
    api::SystemSharing sharing,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        SYSTEM_UPDATE_SHARING_PATH,
        std::move(sharing),
        completionHandler,
        completionHandler);
}

}   //cl
}   //cdb
}   //nx
