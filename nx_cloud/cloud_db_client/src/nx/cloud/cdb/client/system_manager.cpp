#include "system_manager.h"

#include "cdb_request_path.h"
#include "data/system_data.h"

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/utils/app_info.h>

namespace nx {
namespace cdb {
namespace client {

SystemManager::SystemManager(
    network::cloud::CloudModuleUrlFetcher* const cloudModuleEndPointFetcher)
    :
    AsyncRequestsExecutor(cloudModuleEndPointFetcher)
{
}

void SystemManager::bindSystem(
    api::SystemRegistrationData registrationData,
    std::function<void(api::ResultCode, api::SystemData)> completionHandler)
{
    if (registrationData.customization.empty())
        registrationData.customization = nx::utils::AppInfo::customizationName().toStdString();
    executeRequest(
        kSystemBindPath,
        std::move(registrationData),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemData()));
}

void SystemManager::unbindSystem(
    const std::string& systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        kSystemUnbindPath,
        api::SystemId(systemId),
        completionHandler,
        completionHandler);
}

void SystemManager::getSystems(
    std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler)
{
    api::Filter filter;
    filter.nameToValue.emplace(
        api::FilterField::customization,
        nx::utils::AppInfo::customizationName().toStdString());
    getSystemsFiltered(filter, std::move(completionHandler));
}

void SystemManager::getSystemsFiltered(
    const api::Filter& filter,
    std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler)
{
    executeRequest(
        kSystemGetPath,
        filter,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemDataExList()));
}

void SystemManager::getSystem(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler)
{
    executeRequest(
        kSystemGetPath,
        api::SystemId(systemId),
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

void SystemManager::getCloudUsersOfSystem(
    std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler)
{
    executeRequest(
        kSystemGetCloudUsersPath,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemSharingExList()));
}

void SystemManager::getCloudUsersOfSystem(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler)
{
    executeRequest(
        kSystemGetCloudUsersPath,
        api::SystemId(systemId),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemSharingExList()));
}

void SystemManager::getAccessRoleList(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler)
{
    executeRequest(
        kSystemGetAccessRoleListPath,
        api::SystemId(systemId),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemAccessRoleList()));
}

void SystemManager::update(
    const api::SystemAttributesUpdate& updatedData,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        kSystemUpdatePath,
        updatedData,
        completionHandler,
        completionHandler);
}

void SystemManager::rename(
    const std::string& systemId,
    const std::string& systemName,
    std::function<void(api::ResultCode)> completionHandler)
{
    api::SystemAttributesUpdate data;
    data.systemId = systemId;
    data.name = systemName;

    executeRequest(
        kSystemRenamePath,
        std::move(data),
        completionHandler,
        completionHandler);
}

void SystemManager::recordUserSessionStart(
    const std::string& systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    api::UserSessionDescriptor userSessionDescriptor;
    userSessionDescriptor.systemId = systemId;

    executeRequest(
        kSystemRecordUserSessionStartPath,
        std::move(userSessionDescriptor),
        completionHandler,
        completionHandler);
}

void SystemManager::getSystemHealthHistory(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler)
{
    executeRequest(
        kSystemHealthHistoryPath,
        api::SystemId(systemId),
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::SystemHealthHistory()));
}

void SystemManager::startMerge(
    const std::string& idOfSystemToMergeTo,
    const std::string& idOfSystemBeingMerged,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(
            kSystemsMergedToASpecificSystem, {idOfSystemToMergeTo}).c_str(),
        api::SystemId(idOfSystemBeingMerged),
        completionHandler,
        completionHandler);
}

} // namespace client
} // namespace cdb
} // namespace nx
