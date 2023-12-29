// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <string>

#include <nx/cloud/db/api/result_code.h>
#include <nx/network/http/generic_api_client.h>

#include "api/organization.h"
#include "api/system_data.h"
#include "api/user_data.h"

namespace nx::cloud::cps {

namespace api {

using Result = nx::cloud::db::api::Result;

} // namespace api

/**
 * Descriptor for the channel partner API client class. See nx::network::http::GenericApiClient
 * for details.
 */
struct ApiResultCodeDescriptor
{
    using ResultCode = api::Result;

    template<typename... Output>
    static api::Result getResultCode(
        SystemError::ErrorCode systemErrorCode,
        const nx::network::http::Response* response,
        const nx::network::http::ApiRequestResult& fusionRequestResult,
        const Output&...)
    {
        if (systemErrorCode != SystemError::noError)
            return systemErrorCodeToResultCode(systemErrorCode);

        if (!response)
            return {nx::cloud::db::api::ResultCode::networkError};

        return getResultCodeFromResponse(*response, fusionRequestResult);
    }

private:
    static api::Result systemErrorCodeToResultCode(SystemError::ErrorCode systemErrorCode);

    static api::Result getResultCodeFromResponse(
        const network::http::Response& response,
        const nx::network::http::ApiRequestResult& fusionRequestResult);
};

//-------------------------------------------------------------------------------------------------

/**
 * Client for Channel Partner Service API.
 * Requests must be authorized with an OAUTH2 token issued by the cloud authorization server.
 * In most cases, it is cloud user's token.
 * All users mentioned here are cloud accounts registered in the CDB and added to an organization
 * or a channel partner.
 */
class ChannelPartnerClient:
    public nx::network::http::GenericApiClient<ApiResultCodeDescriptor>
{
    using base_type = nx::network::http::GenericApiClient<ApiResultCodeDescriptor>;

public:
    ChannelPartnerClient(const nx::utils::Url& baseApiUrl);
    ~ChannelPartnerClient();

    void bindSystemToOrganization(
        api::SystemRegistrationRequest data,
        nx::utils::MoveOnlyFunc<void(api::Result, api::SystemRegistrationResponse)> handler);

    void unbindSystemFromOrganization(
        std::string systemId,
        nx::utils::MoveOnlyFunc<void(api::Result)> handler);

    /**
     * Returns user access rights on a system. Note that these are organization-provided rights.
     */
    void getSystemUser(
        const std::string& systemId,
        const std::string& email,
        nx::utils::MoveOnlyFunc<void(api::Result, api::User)> handler);

    /**
     * Get users of a system as specified by an organization owning the system or channel partner.
     * Note that local VMS users and regular Cloud users added via CDB API are not included here.
     */
    void getSystemUsers(
        const std::string& systemId,
        nx::utils::MoveOnlyFunc<void(api::Result, std::vector<api::User>)> handler);

    /**
     * Get organization-owned or channel partner-accessible systems a specified user has access to.
     */
    void getUserSystems(
        const std::string& email,
        nx::utils::MoveOnlyFunc<void(api::Result, std::vector<api::SystemAllowance>)> handler);

    /**
     * Get organization attributes by id. Provides requesting user rights on the organization.
     */
    void getOrganization(
        const std::string& organizationId,
        nx::utils::MoveOnlyFunc<void(api::Result, api::Organization)> handler);
};

} // namespace nx::cloud::cps
