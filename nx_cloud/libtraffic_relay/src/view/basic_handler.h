#pragma once

#include <tuple>

#include <nx/network/http/server/abstract_fusion_request_handler.h>
#include <nx/utils/type_utils.h>

#include "../controller/connect_session_manager.h"

namespace nx {
namespace cloud {
namespace relay {
namespace view {

template<typename Request, typename RequestBodyData, typename CompletionHandler, typename ... Response>
class BasicHandler:
    public nx_http::AbstractFusionRequestHandler<
        RequestBodyData,
        typename nx::utils::tuple_first_element<void, std::tuple<Response...>>::type>
{
    using Manager = controller::AbstractConnectSessionManager;
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
        api::ResultCode resultCode,
        Response ... response)
    {
        auto requestResult = api::resultCodeToFusionRequestResult(resultCode);
        if (resultCode == api::ResultCode::needRedirect)
            requestResult.setHttpStatusCode(nx_http::StatusCode::found);

        this->requestCompleted(requestResult, std::move(response)...);
    }

    void onRequestProcessed(
        api::ResultCode resultCode,
        Response ... response,
        nx_http::ConnectionEvents connectionEvents)
    {
        this->setConnectionEvents(std::move(connectionEvents));

        auto requestResult = api::resultCodeToFusionRequestResult(resultCode);
        if (resultCode == api::ResultCode::ok)
            requestResult.setHttpStatusCode(nx_http::StatusCode::switchingProtocols);

        this->requestCompleted(
            std::move(requestResult),
            std::move(response)...);
    }
};

//-------------------------------------------------------------------------------------------------

template<typename Request, typename CompletionHandler, typename ... Response>
class BasicHandlerWithoutRequestBody:
    public BasicHandler<Request, void, CompletionHandler, Response...>
{
    using base_type = BasicHandler<Request, void, CompletionHandler, Response...>;

public:
    template<typename ... Args>
    BasicHandlerWithoutRequestBody(Args ... args):
        base_type(std::move(args)...)
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        const nx_http::Request& /*request*/,
        nx::utils::stree::ResourceContainer /*authInfo*/) override
    {
        Request inputData = prepareRequestData();
        this->invokeManagerFunc(inputData, this->m_managerFunc);
    }

protected:
    virtual Request prepareRequestData() = 0;
};

//-------------------------------------------------------------------------------------------------

template<typename Request, typename CompletionHandler, typename ... Response>
class BasicHandlerWithRequestBody:
    public BasicHandler<Request, Request, CompletionHandler, Response...>
{
    using base_type = BasicHandler<Request, Request, CompletionHandler, Response...>;

public:
    template<typename ... Args>
    BasicHandlerWithRequestBody(Args ... args):
        base_type(std::move(args)...)
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        const nx_http::Request& /*request*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        Request inputData) override
    {
        prepareRequestData(&inputData);
        this->invokeManagerFunc(inputData, this->m_managerFunc);
    }

protected:
    virtual void prepareRequestData(Request* request) = 0;
};

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
