/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "system_manager.h"

#include "cdb_request_path.h"
#include "data/system_data.h"

#include <utils/common/app_info.h>

namespace nx {
namespace cdb {
namespace cl {

SystemManager::SystemManager(network::cloud::CloudModuleEndPointFetcher* const cloudModuleEndPointFetcher)
:
    AsyncRequestsExecutor(cloudModuleEndPointFetcher)
{
}

void SystemManager::bindSystem(
    api::SystemRegistrationData registrationData,
    std::function<void(api::ResultCode, api::SystemData)> completionHandler)
{
    registrationData.customization = QnAppInfo::customizationName().toStdString();
    executeRequest(
        kSystemBindPath,
        std::move(registrationData),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemData()));
}

void SystemManager::unbindSystem(
    const std::string& systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        kSystemUnbindPath,
        api::SystemID(systemID),
        completionHandler,
        completionHandler);
}

void SystemManager::getSystems(
    std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler)
{
    executeRequest(
        kSystemGetPath,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemDataExList()));
}

void SystemManager::getSystem(
    const std::string& systemID,
    std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler)
{
    executeRequest(
        kSystemGetPath,
        api::SystemID(systemID),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemDataExList()));
}

void SystemManager::shareSystem(
    api::SystemSharing sharingData,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        kSystemSharePath,
        std::move(sharingData),
        completionHandler,
        completionHandler);
}

void SystemManager::setSystemUserList(
    api::SystemSharingList sharings,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        kSystemSetSystemUserListPath,
        std::move(sharings),
        completionHandler,
        completionHandler);
}

void SystemManager::getCloudUsersOfSystem(
    std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler)
{
    executeRequest(
        kSystemGetCloudUsersPath,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemSharingExList()));
}

void SystemManager::getCloudUsersOfSystem(
    const std::string& systemID,
    std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler)
{
    executeRequest(
        kSystemGetCloudUsersPath,
        api::SystemID(systemID),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemSharingExList()));
}

void SystemManager::getAccessRoleList(
    const std::string& systemID,
    std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler)
{
    executeRequest(
        kSystemGetAccessRoleListPath,
        api::SystemID(systemID),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemAccessRoleList()));
}

void SystemManager::updateSystemName(
    const std::string& systemID,
    const std::string& systemName,
    std::function<void(api::ResultCode)> completionHandler)
{
    api::SystemNameUpdate data;
    data.id = systemID;
    data.name = systemName;

    executeRequest(
        kSystemUpdateSystemNamePath,
        std::move(data),
        completionHandler,
        completionHandler);
}

}   //cl
}   //cdb
}   //nx
