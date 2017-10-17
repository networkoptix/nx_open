#pragma once

#include <tuple>

#include <nx/network/cloud/tunnel/relay/api/relay_api_result_code.h>
#include <nx/network/http/server/abstract_fusion_request_handler.h>
#include <nx/utils/type_utils.h>

namespace nx {
namespace cloud {
namespace relaying {

template<
    typename Manager,
    typename Request,
    typename RequestBodyData,
    typename CompletionHandler,
    typename ... Response>
class BasicHandler:
    public nx_http::AbstractFusionRequestHandler<
        RequestBodyData,
        typename nx::utils::tuple_first_element<void, std::tuple<Response...>>::type>
{
    using ManagerFunc = void(Manager::*)(const Request&, CompletionHandler);

public:
    BasicHandler(
        Manager* manager,
        ManagerFunc managerFunc)
        :
        m_manager(manager),
        m_managerFunc(managerFunc)
    {
    }

protected:
    Manager* m_manager;
    ManagerFunc m_managerFunc;

    template<typename ... Args>
    void invokeManagerFunc(
        const Request& inputData,
        void(Manager::*managerFunc)(
            const Request&,
            nx::utils::MoveOnlyFunc<void(Args...)>))
    {
        (m_manager->*managerFunc)(
            std::move(inputData),
            [this](Args ... args)
            {
                onRequestProcessed(std::move(args)...);
            });
    }

private:
    void onRequestProcessed(
        relay::api::ResultCode resultCode,
        Response ... response)
    {
        auto requestResult = relay::api::resultCodeToFusionRequestResult(resultCode);
        if (resultCode == relay::api::ResultCode::needRedirect)
            requestResult.setHttpStatusCode(nx_http::StatusCode::found);

        this->requestCompleted(requestResult, std::move(response)...);
    }

    void onRequestProcessed(
        relay::api::ResultCode resultCode,
        Response ... response,
        nx_http::ConnectionEvents connectionEvents)
    {
        this->setConnectionEvents(std::move(connectionEvents));

        auto requestResult = relay::api::resultCodeToFusionRequestResult(resultCode);
        if (resultCode == relay::api::ResultCode::ok)
            requestResult.setHttpStatusCode(nx_http::StatusCode::switchingProtocols);

        this->requestCompleted(
            std::move(requestResult),
            std::move(response)...);
    }
};

//-------------------------------------------------------------------------------------------------

template<typename Manager, typename Request, typename CompletionHandler, typename ... Response>
class BasicHandlerWithoutRequestBody:
    public BasicHandler<Manager, Request, void, CompletionHandler, Response...>
{
    using base_type = BasicHandler<Manager, Request, void, CompletionHandler, Response...>;

public:
    template<typename ... Args>
    BasicHandlerWithoutRequestBody(Args ... args):
        base_type(std::move(args)...)
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer /*authInfo*/) override
    {
        Request inputData = prepareRequestData(connection, request);
        this->invokeManagerFunc(inputData, this->m_managerFunc);
    }

protected:
    virtual Request prepareRequestData(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& httpRequest) = 0;
};

//-------------------------------------------------------------------------------------------------

template<typename Manager, typename Request, typename CompletionHandler, typename ... Response>
class BasicHandlerWithRequestBody:
    public BasicHandler<Manager, Request, Request, CompletionHandler, Response...>
{
    using base_type = BasicHandler<Manager, Request, Request, CompletionHandler, Response...>;

public:
    template<typename ... Args>
    BasicHandlerWithRequestBody(Args ... args):
        base_type(std::move(args)...)
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        Request inputData) override
    {
        prepareRequestData(connection, request, &inputData);
        this->invokeManagerFunc(inputData, this->m_managerFunc);
    }

protected:
    virtual void prepareRequestData(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& httpRequest,
        Request* request) = 0;
};

} // namespace relaying
} // namespace cloud
} // namespace nx
