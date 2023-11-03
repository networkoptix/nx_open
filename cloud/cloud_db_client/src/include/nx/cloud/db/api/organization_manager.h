// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <string>

#include "result_code.h"
#include "system_data.h"

namespace nx::cloud::db::api {

/**
 * Contains actions available on organizations.
 */
class OrganizationManager
{
public:
    virtual ~OrganizationManager() = default;

    /**
     * Create new system and bind it to organization.
     * The calling user must the corresponding rights on the organization.
     */
    virtual void bindSystem(
        const std::string& organizationId,
        SystemRegistrationData registrationData,
        std::function<void(ResultCode, SystemData)> completionHandler) = 0;

    /**
     * Get all systems bound to the given organization.
     */
    virtual void getSystems(
        const std::string& organizationId,
        std::function<void(api::ResultCode, std::vector<api::SystemDataEx>)> completionHandler) = 0;

    /**
     * Remove organization-owned system.
     * The calling user must the corresponding rights on the organization.
     */
    virtual void removeSystem(
        const std::string& organizationId,
        const std::string& systemId,
        std::function<void(ResultCode)> completionHandler) = 0;

    virtual void shareSystem(
        const std::string& organizationId,
        const std::string& systemId,
        const api::ShareSystemRequest& sharing,
        std::function<void(api::ResultCode, api::ShareSystemRequest)> completionHandler) = 0;
};

} // namespace nx::cloud::db::api
