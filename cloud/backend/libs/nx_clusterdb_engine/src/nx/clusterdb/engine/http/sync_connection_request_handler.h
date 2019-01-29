#pragma once

#include <string>

#include <nx/network/http/http_types.h>
#include <nx/network/http/server/abstract_http_request_handler.h>

#include "http_paths.h"

namespace nx::clusterdb::engine {

NX_DATA_SYNC_ENGINE_API std::string extractSystemIdFromHttpRequest(
    const nx::network::http::RequestContext& requestContext);

template<typename ManagerType>
class SyncConnectionRequestHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    using ManagerFuncType = void (ManagerType::*)(
        nx::network::http::RequestContext requestContext,
        const std::string& systemId,
        nx::network::http::RequestProcessedHandler completionHandler);

    SyncConnectionRequestHandler(
        ManagerType* manager,
        ManagerFuncType managerFuncPtr)
        :
        m_manager(manager),
        m_managerFuncPtr(managerFuncPtr)
    {
    }

protected:
    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override
    {
        std::string systemId = extractSystemIdFromHttpRequest(requestContext);

        (m_manager->*m_managerFuncPtr)(
            std::move(requestContext),
            systemId,
            std::move(completionHandler));
    }

private:
    ManagerType* m_manager;
    ManagerFuncType m_managerFuncPtr;
};

} // namespace nx::clusterdb::engine
