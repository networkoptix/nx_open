#pragma once

#include <string>

#include <nx/network/http/http_types.h>
#include <nx/network/http/server/abstract_http_request_handler.h>

#include "http_paths.h"

namespace nx::clusterdb::engine {

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
        std::string systemId = extractUserName(requestContext);

        (m_manager->*m_managerFuncPtr)(
            std::move(requestContext),
            systemId,
            std::move(completionHandler));
    }

private:
    ManagerType* m_manager;
    ManagerFuncType m_managerFuncPtr;

    std::string extractUserName(
        const nx::network::http::RequestContext& requestContext) const
    {
        using namespace nx::network::http;

        const auto systemId = 
            requestContext.requestPathParams.getByName(kSystemIdParamName);
        if (!systemId.empty())
            return systemId;

        const auto& request = requestContext.request;

        auto authorizationHeaderIter = request.headers.find(header::Authorization::NAME);
        if (authorizationHeaderIter == request.headers.end())
            return std::string();

        header::Authorization authorization;
        if (!authorization.parse(authorizationHeaderIter->second))
            return std::string();

        return authorization.userid().toStdString();
    }
};

} // namespace nx::clusterdb::engine
