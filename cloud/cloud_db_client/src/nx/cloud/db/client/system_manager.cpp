// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_manager.h"

#include <nx/branding.h>
#include <nx/network/http/rest/http_rest_client.h>

#include "cdb_request_path.h"
#include "data/system_data.h"

namespace nx::cloud::db::client {

SystemManager::SystemManager(
    AsyncRequestsExecutor* requestsExecutor)
    :
    m_requestsExecutor(requestsExecutor)
{
}

void SystemManager::bindSystem(
    api::SystemRegistrationData registrationData,
    std::function<void(api::ResultCode, api::SystemData)> completionHandler)
{
    if (registrationData.customization.empty())
        registrationData.customization = nx::branding::customization().toStdString();

    m_requestsExecutor->executeRequest<api::SystemData>(
        nx::network::http::Method::post,
        kSystemBindPath,
        std::move(registrationData),
        std::move(completionHandler));
}

void SystemManager::unbindSystem(
    const std::string& systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest</*Output*/ void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(kSystemPath, {systemId}),
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
    m_requestsExecutor->executeRequest<api::SystemDataExList>(
        nx::network::http::Method::get,
        kSystemsPath,
        filter,
        std::move(completionHandler));
}

void SystemManager::getSystem(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::SystemDataEx)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::SystemDataEx>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(kSystemPath, {systemId}),
        std::move(completionHandler));
}

void SystemManager::getSystems(
    api::SystemIdList systemIdList,
    std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::SystemDataExList>(
        nx::network::http::Method::get,
        kSystemsPath,
        std::move(systemIdList),
        std::move(completionHandler));
}

void SystemManager::shareSystem(
    const std::string& systemId,
    api::ShareSystemRequest sharingData,
    std::function<void(api::ResultCode, api::SystemSharing)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::SystemSharing>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(kSystemUsersPath, {systemId}),
        std::move(sharingData),
        std::move(completionHandler));
}

void SystemManager::revokeUserAccess(
    const std::string& systemId,
    const std::string& email,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest</*Output*/ void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(kSystemUserPath, {systemId, email}),
        std::move(completionHandler));
}

void SystemManager::getCloudUsersOfSystem(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::SystemSharingList)> completionHandler)
{
    m_requestsExecutor->executeRequest<std::vector<api::SystemSharing>>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(kSystemUsersPath, {systemId}),
        [completionHandler = std::move(completionHandler)](
            api::ResultCode resultCode, std::vector<api::SystemSharing> systems)
        {
            completionHandler(resultCode, api::SystemSharingList{std::move(systems)});
        });
}

void SystemManager::saveCloudUserOfSystem(
    const std::string& systemId,
    const api::SystemSharing& userData,
    std::function<void(api::ResultCode, api::SystemSharing)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::SystemSharing>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(kSystemUsersPath, {systemId}),
        userData,
        std::move(completionHandler));
}

void SystemManager::getAccessRoleList(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::SystemAccessRoleList>(
        nx::network::http::Method::get,
        deprecated::kSystemGetAccessRoleListPath,
        api::SystemId(systemId),
        std::move(completionHandler));
}

void SystemManager::update(
    const std::string& systemId,
    const api::SystemAttributesUpdate& updatedData,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest</*Output*/ void>(
        nx::network::http::Method::put,
        nx::network::http::rest::substituteParameters(kSystemPath, {systemId}),
        updatedData,
        std::move(completionHandler));
}

void SystemManager::rename(
    const std::string& systemId,
    const std::string& name,
    std::function<void(api::ResultCode)> completionHandler)
{
    update(systemId, api::SystemAttributesUpdate{.name = name}, std::move(completionHandler));
}

void SystemManager::recordUserSessionStart(
    const std::string& systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    api::UserSessionDescriptor userSessionDescriptor;
    userSessionDescriptor.systemId = systemId;

    m_requestsExecutor->executeRequest</*Output*/ void>(
        nx::network::http::Method::post,
        kSystemRecordUserSessionStartPath,
        std::move(userSessionDescriptor),
        std::move(completionHandler));
}

void SystemManager::getSystemHealthHistory(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::SystemHealthHistory>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(
            kSystemHealthHistoryPath, {systemId}),
        std::move(completionHandler));
}

void SystemManager::getSystemDataSyncSettings(
    const std::string& systemId,
    std::function<void(api::ResultCode, api::DataSyncSettings)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::DataSyncSettings>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(kSystemDataSyncSettingsPath, {systemId}),
        std::move(completionHandler));
}

void SystemManager::startMerge(
    const std::string& idOfSystemToMergeTo,
    const std::string& idOfSystemBeingMerged,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest</*Output*/ void>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(
            kSystemsMergedToASpecificSystem, {idOfSystemToMergeTo}),
        api::SystemId(idOfSystemBeingMerged),
        std::move(completionHandler));
}

void SystemManager::startMerge(
    const std::string& idOfSystemToMergeTo,
    const api::MergeRequest& request,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest</*Output*/ void>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(
            kSystemsMergedToASpecificSystem, {idOfSystemToMergeTo}),
        request,
        std::move(completionHandler));
}

void SystemManager::validateMSSignature(
    const std::string& systemId,
    const api::ValidateMSSignatureRequest& request,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest<void>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(
            kSystemsValidateMSSignature, {systemId}),
        request,
        std::move(completionHandler));
}

void SystemManager::offerSystem(
    api::CreateSystemOfferRequest request,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest</*Reply*/ void>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(kOfferSystemOwnership, {request.systemId}),
        request,
        std::move(completionHandler));
}

void SystemManager::acceptSystemOffer(
    const std::string& systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    api::SystemOfferPatch request;
    request.status = api::OfferStatus::accepted;

    m_requestsExecutor->executeRequest</*Reply*/ void>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(kSystemOwnershipOfferAccept, {systemId}),
        std::move(request),
        std::move(completionHandler));
}

void SystemManager::rejectSystemOffer(
    const std::string& systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    api::SystemOfferPatch request;
    request.status = api::OfferStatus::rejected;

    m_requestsExecutor->executeRequest</*Reply*/ void>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(kSystemOwnershipOfferReject, {systemId}),
        std::move(request),
        std::move(completionHandler));
}

void SystemManager::deleteSystemOffer(
    const std::string& systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest</*Reply*/ void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(kOfferSystemOwnership, {systemId}),
        std::move(completionHandler));
}

void SystemManager::getSystemOffers(
    std::function<void(api::ResultCode, std::vector<api::SystemOffer>)> completionHandler)
{
    m_requestsExecutor->executeRequest<std::vector<api::SystemOffer>>(
        nx::network::http::Method::get,
        kSystemOwnershipOffers,
        std::move(completionHandler));
}

void SystemManager::addSystemAttributes(
    const std::string& systemId,
    const std::vector<api::Attribute>& attributes,
    std::function<void(api::ResultCode, std::vector<api::Attribute>)> completionHandler)
{
    m_requestsExecutor->executeRequest<std::vector<api::Attribute>>(
        nx::network::http::Method::post,
         nx::network::http::rest::substituteParameters(
            kSystemAttributesPath, {systemId}),
        std::move(attributes),
        std::move(completionHandler));
}

void SystemManager::updateSystemAttributes(
    const std::string& systemId,
    const std::vector<api::Attribute>& attributes,
    std::function<void(api::ResultCode, std::vector<api::Attribute>)> completionHandler)
{
    m_requestsExecutor->executeRequest<std::vector<api::Attribute>>(
        nx::network::http::Method::put,
         nx::network::http::rest::substituteParameters(
            kSystemAttributesPath, {systemId}),
        std::move(attributes),
        std::move(completionHandler));
}

void SystemManager::addSystemAttribute(
    const std::string& systemId,
    const api::Attribute& attribute,
    std::function<void(api::ResultCode, api::Attribute)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::Attribute>(
        nx::network::http::Method::post,
        nx::network::http::rest::substituteParameters(
            kSystemAttributePath, {systemId, attribute.name}),
        std::move(attribute),
        std::move(completionHandler));
}

void SystemManager::updateSystemAttribute(
    const std::string& systemId,
    const api::Attribute& attribute,
    std::function<void(api::ResultCode, api::Attribute)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::Attribute>(
        nx::network::http::Method::put,
        nx::network::http::rest::substituteParameters(kSystemAttributePath, {systemId, attribute.name}),
        std::move(attribute),
        std::move(completionHandler));
}

void SystemManager::getSystemAttributes(
    const std::string& systemId,
    std::function<void(api::ResultCode, std::vector<api::Attribute>)> completionHandler)
{
    m_requestsExecutor->executeRequest<std::vector<api::Attribute>>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(
            kSystemAttributesPath, {systemId}),
        std::move(completionHandler));
}

void SystemManager::deleteSystemAttribute(
    const std::string& systemId,
    const std::string& attrName,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest</*Reply*/ void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(kSystemAttributePath, {systemId, attrName}),
        std::move(completionHandler));
}

void SystemManager::updateUserAttributes(
    const std::string& systemId,
    const std::string& email,
    const std::vector<api::Attribute>& attributes,
    std::function<void(api::ResultCode, std::vector<api::Attribute>)> completionHandler)
{
    m_requestsExecutor->executeRequest<std::vector<api::Attribute>>(
        nx::network::http::Method::put,
         nx::network::http::rest::substituteParameters(
            kSystemUserAttributesPath, {systemId, email}),
        std::move(attributes),
        std::move(completionHandler));
}

void SystemManager::getUserAttributes(
    const std::string& systemId,
    const std::string& email,
    std::function<void(api::ResultCode, std::vector<api::Attribute>)> completionHandler)
{
    m_requestsExecutor->executeRequest<std::vector<api::Attribute>>(
        nx::network::http::Method::get,
        nx::network::http::rest::substituteParameters(
            kSystemUserAttributesPath, {systemId, email}),
        std::move(completionHandler));
}

void SystemManager::updateUserAttribute(
    const std::string& systemId,
    const std::string& email,
    const api::Attribute& attribute,
    std::function<void(api::ResultCode, api::Attribute)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::Attribute>(
        nx::network::http::Method::put,
        nx::network::http::rest::substituteParameters(
            kSystemUserAttributePath, {systemId, email, attribute.name}),
        std::move(attribute),
        std::move(completionHandler));
}

void SystemManager::deleteUserAttribute(
    const std::string& systemId,
    const std::string& email,
    const std::string& attrName,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest</*Reply*/ void>(
        nx::network::http::Method::delete_,
        nx::network::http::rest::substituteParameters(
            kSystemUserAttributePath, {systemId, email, attrName}),
        std::move(completionHandler));
}

} // namespace nx::cloud::db::client
