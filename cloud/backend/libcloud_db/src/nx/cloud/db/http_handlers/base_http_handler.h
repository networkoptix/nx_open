#pragma once

#include <nx/network/aio/timer.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/server/abstract_fusion_request_handler.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_server_connection.h>
#include <nx/utils/std/cpp14.h>

#include <nx/cloud/db/client/data/types.h>
#include <nx/fusion/model_functions.h>

#include "../access_control/authorization_manager.h"
#include "../access_control/auth_types.h"
#include "../access_control/security_manager.h"
#include "../access_control/access_blocker.h"
#include "../managers/managers_types.h"
#include "../stree/cdb_ns.h"
#include "../stree/http_request_attr_reader.h"
#include "../stree/socket_attr_reader.h"

namespace nx::cloud::db {
namespace detail {

template<typename Input = void, typename Output = void>
class BasicHttpRequestHandler:
    public nx::network::http::AbstractFusionRequestHandler<Input, Output>
{
    using base_type = 
        nx::network::http::AbstractFusionRequestHandler<Input, Output>;

public:
    BasicHttpRequestHandler(
        EntityType entityType,
        DataActionType actionType,
        const SecurityManager& securityManager)
        :
        m_entityType(entityType),
        m_actionType(actionType),
        m_securityManager(securityManager)
    {
    }

protected:
    bool authorize(
        const nx::network::http::RequestContext& requestContext,
        const nx::utils::stree::AbstractResourceReader& dataToAuthorize,
        nx::utils::stree::ResourceContainer* const authzInfo)
    {
        const auto& authenticationData = requestContext.authInfo;

        SocketResourceReader socketResources(*requestContext.connection->socket());
        HttpRequestResourceReader httpRequestResources(requestContext.request);

        // Performing authorization.
        // Authorization is performed here since it can depend on input data which
        // needs to be deserialized depending on request type.
        if (!m_securityManager.authorizer().authorize(
                authenticationData,
                nx::utils::stree::MultiSourceResourceReader(
                    socketResources,
                    httpRequestResources,
                    dataToAuthorize),
                this->m_entityType,
                this->m_actionType,
                authzInfo)) //< Using same object since we can move it.
        {
            api::ResultCode resultCode = api::ResultCode::forbidden;
            if (auto resultCodeStr = authzInfo->get<QString>(attr::resultCode))
            {
                resultCode = QnLexical::deserialized<api::ResultCode>(
                    *resultCodeStr,
                    api::ResultCode::unknownError);
            }

            nx::network::http::FusionRequestResult result(
                nx::network::http::FusionRequestErrorClass::unauthorized,
                QnLexical::serialized(resultCode),
                static_cast<int>(resultCode),
                QString());

            this->response()->headers.emplace(
                Qn::API_RESULT_CODE_HEADER_NAME,
                QnLexical::serialized(resultCode).toLatin1());

            this->requestCompleted(std::move(result));
            return false;
        }
        return true;
    }

private:
    const EntityType m_entityType;
    const DataActionType m_actionType;
    const SecurityManager& m_securityManager;
};

} // namespace detail

//-------------------------------------------------------------------------------------------------

template<typename Input, typename ... Output>
class AbstractFiniteMsgBodyHttpHandler:
    public detail::BasicHttpRequestHandler<Input, Output...>
{
    static_assert(sizeof...(Output) <= 1, "Specify output data type or leave blank");

public:
    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const SecurityManager& securityManager)
        :
        detail::BasicHttpRequestHandler<Input, Output...>(
            entityType,
            actionType,
            securityManager)
    {
    }

    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        Input inputData) override
    {
        if (!this->authorize(requestContext, inputData, &requestContext.authInfo))
            return;

        processRequest(
            std::move(requestContext),
            std::move(inputData),
            [this](api::Result result, Output... outData)
            {
                this->response()->headers.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(result.code).toLatin1());

                this->requestCompleted(
                    apiResultToFusionRequestResult(result),
                    std::move(outData)...);
            });
    }

protected:
    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        Input inputData,
        std::function<void(api::Result, Output...)> completionHandler) = 0;
};

/**
 * Contains logic common for all cloud_db HTTP request handlers.
 */
template<typename Input, typename ... Output>
class FiniteMsgBodyHttpHandler:
    public AbstractFiniteMsgBodyHttpHandler<Input, Output...>
{
    static_assert(sizeof...(Output) <= 1, "Specify output data type or leave blank");

public:
    typedef std::function<void(
        const AuthorizationInfo& authzInfo,
        Input inputData,
        std::function<void(api::Result result, Output... outData)>&& completionHandler)
    > ExecuteRequestFunc;

    FiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const SecurityManager& securityManager,
        ExecuteRequestFunc requestFunc)
        :
        AbstractFiniteMsgBodyHttpHandler<Input, Output...>(
            entityType,
            actionType,
            securityManager),
        m_requestFunc(std::move(requestFunc))
    {
    }

protected:
    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        Input inputData,
        std::function<void(api::Result, Output...)> completionHandler) override
    {
        m_requestFunc(
            AuthorizationInfo(std::exchange(requestContext.authInfo, {})),
            std::move(inputData),
            std::move(completionHandler));
    }

private:
    ExecuteRequestFunc m_requestFunc;
};

//-------------------------------------------------------------------------------------------------

/**
 * No input.
 */
template<typename... Output>
class AbstractFiniteMsgBodyHttpHandler<void, Output...>:
    public detail::BasicHttpRequestHandler<void, Output...>
{
    static_assert(sizeof...(Output) <= 1, "Specify output data type or leave blank");

public:
    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const SecurityManager& securityManager)
        :
        detail::BasicHttpRequestHandler<void, Output...>(
            entityType,
            actionType,
            securityManager)
    {
    }

    virtual void processRequest(
        nx::network::http::RequestContext requestContext) override
    {
        if (!this->authorize(
                requestContext,
                nx::utils::stree::ResourceContainer(),
                &requestContext.authInfo))
        {
            return;
        }

        processRequest(
            std::move(requestContext),
            [this](api::Result result, Output... outData)
            {
                this->response()->headers.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(result.code).toLatin1());

                this->requestCompleted(
                    apiResultToFusionRequestResult(result),
                    std::move(outData)...);
            });
    }

protected:
    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        std::function<void(api::Result, Output...)> completionHandler) = 0;
};

template<typename... Output>
class FiniteMsgBodyHttpHandler<void, Output...>:
    public AbstractFiniteMsgBodyHttpHandler<void, Output...>
{
public:
    typedef std::function<void(
        const AuthorizationInfo& authzInfo,
        std::function<void(api::Result /*result*/, Output... /*outData*/)> completionHandler)
    > ExecuteRequestFunc;

    FiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const SecurityManager& securityManager,
        ExecuteRequestFunc requestFunc)
        :
        AbstractFiniteMsgBodyHttpHandler<void, Output...>(
            entityType,
            actionType,
            securityManager),
        m_requestFunc(std::move(requestFunc))
    {
    }

protected:
    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        std::function<void(api::Result, Output...)> completionHandler) override
    {
        m_requestFunc(
            AuthorizationInfo(std::exchange(requestContext.authInfo, {})),
            std::move(completionHandler));
    }

private:
    ExecuteRequestFunc m_requestFunc;
};

//-------------------------------------------------------------------------------------------------

template<typename... InputData>
class AbstractFreeMsgBodyHttpHandler:
    public detail::BasicHttpRequestHandler<InputData...>
{
    static_assert(sizeof...(InputData) <= 1, "Specify input data type or leave blank");

public:
    typedef nx::utils::MoveOnlyFunc<void(
        nx::network::http::HttpServerConnection* const connection,
        const AuthorizationInfo& authzInfo,
        InputData... inputData,
        nx::utils::MoveOnlyFunc<
        void(api::Result, std::unique_ptr<nx::network::http::AbstractMsgBodySource>)>
            completionHandler)> ExecuteRequestFunc;

    AbstractFreeMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const SecurityManager& securityManager,
        ExecuteRequestFunc requestFunc)
        :
        detail::BasicHttpRequestHandler<InputData..., void>(
            entityType,
            actionType,
            securityManager),
        m_requestFunc(std::move(requestFunc))
    {
    }

    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        InputData... inputData) override
    {
        if (!this->authorize(
                requestContext,
                nx::utils::stree::ResourceContainer(),
                &requestContext.authInfo))
        {
            return;
        }

        m_requestFunc(
            requestContext.connection,
            AuthorizationInfo(std::exchange(requestContext.authInfo, {})),
            std::move(inputData)...,
            [this](
                api::Result result,
                std::unique_ptr<nx::network::http::AbstractMsgBodySource> responseMsgBody)
            {
                this->response()->headers.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(result.code).toLatin1());

                if (result == api::ResultCode::ok)
                {
                    this->requestCompleted(
                        nx::network::http::StatusCode::ok,
                        std::move(responseMsgBody));
                }
                else
                {
                    this->requestCompleted(
                        apiResultToFusionRequestResult(result));
                }
            });
    }

private:
    ExecuteRequestFunc m_requestFunc;
};

} // namespace nx::cloud::db
