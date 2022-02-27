// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_manager.h"

#include "cdb_request_path.h"
#include "data/system_data.h"

#include <nx/network/http/rest/http_rest_client.h>

#include <nx/branding.h>

namespace nx::cloud::db::client {

SystemManager::SystemManager(
    network::cloud::CloudModuleUrlFetcher* cloudModuleUrlFetcher)
    :
    AsyncRequestsExecutor(cloudModuleUrlFetcher)
{
}

void SystemManager::bindSystem(
    api::SystemRegistrationData registrationData,
    std::function<void(api::ResultCode, api::SystemData)> completionHandler)
{
    if (registrationData.customization.empty())
        registrationData.customization = nx::branding::customization().toStdString();

    executeRequest<api::SystemData>(
        nx::network::http::Method::post,
        kSystemBindPath,
        std::move(registrationData),
        std::move(completionHandler));
}

void SystemManager::unbindSystem(
    const std::string& systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest</*Output*/ void>(
        nx::network::http::Method::post,
        kSystemUnbindPath,
        api::SystemId(systemId),
        std::move(completionHandler));
}

void SystemManager::getSystems(
    std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler)
{
    api::Filter filter;
    filter.nameToValue.emplace(
        api::FilterField::customization,
        nx::branding::customization().toStdString());
    getSystemsFiltered(filter, std::move(completionHandler));
}

void SystemManager::getSystemsFiltered(
    const api::Filter& filter,
    std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler)
{
    executeRequest<api::SystemDataExList>(
        nx::network::http::Method::get,
        kSystemGetPath,
        filter,
        std::move(completionHandler));
}

void SystemManager::getSystem(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler)
{
    executeRequest<api::SystemDataExList>(
        nx::network::http::Method::get,
        kSystemGetPath,
        api::SystemId(systemId),
        std::move(completionHandler));
}

void SystemManager::shareSystem(
    api::SystemSharing sharingData,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest</*Output*/ void>(
        nx::network::http::Method::post,
        kSystemSharePath,
        std::move(sharingData),
        std::move(completionHandler));
}

void SystemManager::getCloudUsersOfSystem(
    std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler)
{
    executeRequest<api::SystemSharingExList>(
        kSystemGetCloudUsersPath,
        std::move(completionHandler));
}

void SystemManager::getCloudUsersOfSystem(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler)
{
    executeRequest<api::SystemSharingExList>(
        nx::network::http::Method::get,
        kSystemGetCloudUsersPath,
        api::SystemId(systemId),
        std::move(completionHandler));
}

void SystemManager::getAccessRoleList(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler)
{
    executeRequest<api::SystemAccessRoleList>(
        nx::network::http::Method::get,
        kSystemGetAccessRoleListPath,
        api::SystemId(systemId),
        std::move(completionHandler));
}

void SystemManager::update(
    const api::SystemAttributesUpdate& updatedData,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest</*Output*/ void>(
        nx::network::http::Method::post,
        kSystemUpdatePath,
        updatedData,
        std::move(completionHandler));
}

void SystemManager::rename(
    const std::string& systemId,
    const std::string& systemName,
    std::function<void(api::ResultCode)> completionHandler)
{
    api::SystemAttributesUpdate data;
    data.systemId = systemId;
    data.name = systemName;

    executeRequest</*Output*/ void>(
        nx::network::http::Method::post,
        kSystemRenamePath,
        std::move(data),
        std::move(completionHandler));
}

void SystemManager::recordUserSessionStart(
    const std::string& systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    api::UserSessionDescriptor userSessionDescriptor;
    userSessionDescriptor.systemId = systemId;

    executeRequest</*Output*/ void>(
        nx::network::http::Method::post,
        kSystemRecordUserSessionStartPath,
        std::move(userSessionDescriptor),
        std::move(completionHandler));
}

void SystemManager::getSystemHealthHistory(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler)
{
    executeRequest<api::SystemHealthHistory>(
        nx::network::http::Method::get,
        kSystemHealthHistoryPath,
        api::SystemId(systemId),
        std::move(completionHandler));
}

void SystemManager::startMerge(
    const std::string& idOfSystemToMergeTo,
    const std::string& idOfSystemBeingMerged,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest</*Output*/ void>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(
            kSystemsMergedToASpecificSystem, {idOfSystemToMergeTo}),
        api::SystemId(idOfSystemBeingMerged),
        std::move(completionHandler));
}

void SystemManager::validateMSSignature(
    const std::string& systemId,
    const api::ValidateMSSignatureRequest& request,
    std::function<void(api::ResultCode)> completionHandler)
{
    executeRequest<void>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(
            kSystemsValidateMSSignature, {systemId}),
        request,
        std::move(completionHandler));
}

} // namespace nx::cloud::db::client
