#pragma once

#include <string>

#include <nx/network/http/http_types.h>
#include <nx/network/http/server/abstract_http_request_handler.h>

namespace nx {
namespace data_sync_engine {

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
        std::string systemId = extractUserName(requestContext.request);

        (m_manager->*m_managerFuncPtr)(
            std::move(requestContext),
            systemId,
            std::move(completionHandler));
    }

private:
    ManagerType* m_manager;
    ManagerFuncType m_managerFuncPtr;

    std::string extractUserName(const nx::network::http::Request& request) const
    {
        using namespace nx::network::http;

        auto authorizationHeaderIter = request.headers.find(header::Authorization::NAME);
        if (authorizationHeaderIter == request.headers.end())
            return std::string();

        header::Authorization authorization;
        if (!authorization.parse(authorizationHeaderIter->second))
            return std::string();

        return authorization.userid().toStdString();
    }
};

} // namespace data_sync_engine
} // namespace nx
