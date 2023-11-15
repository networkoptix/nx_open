// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "organization_manager.h"

#include <nx/network/http/rest/http_rest_client.h>

#include "cdb_request_path.h"
#include "data/system_data.h"

namespace nx::cloud::db::client {

using namespace nx::network::http;

OrganizationManager::OrganizationManager(AsyncRequestsExecutor* requestsExecutor):
    m_requestsExecutor(requestsExecutor)
{
}

void OrganizationManager::bindSystem(
    const std::string& organizationId,
    api::SystemRegistrationData registrationData,
    std::function<void(api::ResultCode, api::SystemData)> completionHandler)
{
    if (registrationData.customization.empty())
        registrationData.customization = nx::branding::customization().toStdString();

    m_requestsExecutor->executeRequest<api::SystemData>(
        Method::post,
        rest::substituteParameters(kOrganizationSystemsPath, {organizationId}),
        std::move(registrationData),
        std::move(completionHandler));
}

void OrganizationManager::getSystems(
    const std::string& organizationId,
    std::function<void(api::ResultCode, std::vector<api::SystemDataEx>)> completionHandler)
{
    m_requestsExecutor->executeRequest<std::vector<api::SystemDataEx>>(
        Method::get,
        rest::substituteParameters(kOrganizationSystemsPath, {organizationId}),
        std::move(completionHandler));
}

void OrganizationManager::removeSystem(
    const std::string& organizationId,
    const std::string& systemId,
    std::function<void(api::ResultCode)> completionHandler)
{
    m_requestsExecutor->executeRequest<void>(
        Method::delete_,
        rest::substituteParameters(kOrganizationSystemPath, {organizationId, systemId}),
        std::move(completionHandler));
}

void OrganizationManager::shareSystem(
    const std::string& organizationId,
    const std::string& systemId,
    const api::ShareSystemRequest& request,
    std::function<void(api::ResultCode, api::SystemSharing)> completionHandler)
{
    m_requestsExecutor->executeRequest<api::SystemSharing>(
        Method::post,
        rest::substituteParameters(kOrganizationSystemUsersPath, {organizationId, systemId}),
        request,
        std::move(completionHandler));
}

} // namespace nx::cloud::db::client
