#pragma once

#include <memory>

#include <nx/vms/applauncher/api/applauncher_api.h>

namespace nx::applauncher {

class AbstractRequestProcessor
{
public:
    virtual ~AbstractRequestProcessor() = default;

    /** It is recommended that implementation does not block! */
    virtual void processRequest(
        const std::shared_ptr<api::BaseTask>& request,
        api::Response** const response) = 0;
};

} // namespace nx::applauncher
