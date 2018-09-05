#pragma once

#include <nx/vms/applauncher/api/applauncher_api.h>

class AbstractRequestProcessor
{
public:
    virtual ~AbstractRequestProcessor() {}

    //!
    /*!
        It is recommended that implementation does not block!
    */
    virtual void processRequest(
        const std::shared_ptr<applauncher::api::BaseTask>& request,
        applauncher::api::Response** const response ) = 0;
};
