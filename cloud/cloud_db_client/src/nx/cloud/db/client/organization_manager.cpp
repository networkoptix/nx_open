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

} // namespace nx::cloud::db::client
