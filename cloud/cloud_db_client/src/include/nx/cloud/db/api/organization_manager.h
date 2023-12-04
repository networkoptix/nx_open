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
     * Get the list of system offers either for the organization or by the organization.
     */
    virtual void getSystemOffers(
        const std::string& organizationId,
        std::function<void(ResultCode, std::vector<api::SystemOffer>)> completionHandler) = 0;

    virtual void acceptOffer(
        const std::string& organizationId,
        const std::string& systemId,
        std::function<void(ResultCode)> completionHandler) = 0;

    virtual void rejectOffer(
        const std::string& organizationId,
        const std::string& systemId,
        std::function<void(ResultCode)> completionHandler) = 0;
};

} // namespace nx::cloud::db::api
