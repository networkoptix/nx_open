#pragma once

#include <nx/network/http/custom_headers.h>
#include <nx/utils/std/cpp14.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/server/abstract_fusion_request_handler.h>
#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_server_connection.h>

#include <nx/cloud/cdb/client/data/types.h>
#include <nx/fusion/model_functions.h>

#include "../access_control/authorization_manager.h"
#include "../access_control/auth_types.h"
#include "../managers/managers_types.h"
#include "../stree/cdb_ns.h"
#include "../stree/http_request_attr_reader.h"
#include "../stree/socket_attr_reader.h"

namespace nx {
namespace cdb {
namespace detail {

template<typename Input, typename Output>
class BaseFiniteMsgBodyHttpHandler:
    public nx_http::AbstractFusionRequestHandler<Input, Output>
{
public:
    BaseFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager)
        :
        m_entityType(entityType),
        m_actionType(actionType),
        m_authorizationManager(authorizationManager)
    {
    }

protected:
    const EntityType m_entityType;
    const DataActionType m_actionType;
    const AuthorizationManager& m_authorizationManager;

    bool authorize(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        const nx::utils::stree::AbstractResourceReader& authenticationData,
        const nx::utils::stree::AbstractResourceReader& dataToAuthorize,
        nx::utils::stree::ResourceContainer* const authzInfo)
    {
        SocketResourceReader socketResources(*connection->socket());
        HttpRequestResourceReader httpRequestResources(request);

        // Performing authorization.
        // Authorization is performed here since it can depend on input data which 
        // needs to be deserialized depending on request type.
        if (!m_authorizationManager.authorize(
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

            nx_http::FusionRequestResult result(
                nx_http::FusionRequestErrorClass::unauthorized,
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
};

} // namespace detail


/**
 * Contains logic common for all cloud_db HTTP request handlers.
 */
template<typename Input = void, typename Output = void>
class AbstractFiniteMsgBodyHttpHandler:
    public detail::BaseFiniteMsgBodyHttpHandler<Input, Output>
{
public:
    typedef std::function<void(
        const AuthorizationInfo& authzInfo,
        Input inputData,
        std::function<void(api::ResultCode resultCode, Output outData)>&& completionHandler)> ExecuteRequestFunc;

    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc requestFunc)
        :
        detail::BaseFiniteMsgBodyHttpHandler<Input, Output>(
            entityType,
            actionType,
            authorizationManager),
        m_requestFunc(std::move(requestFunc))
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer authInfo,
        Input inputData) override
    {
        if (!this->authorize(
                connection,
                request,
                authInfo,
                inputData,
                &authInfo))
            return;

        m_requestFunc(
            AuthorizationInfo(std::move(authInfo)),
            std::move(inputData),
            [this](api::ResultCode resultCode, Output outData)
            {
                this->response()->headers.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(resultCode).toLatin1());
                this->requestCompleted(
                    resultCodeToFusionRequestResult(resultCode),
                    std::move(outData));
            });
    }

private:
    ExecuteRequestFunc m_requestFunc;
};


/**
 * No output/
 */
template<typename Input>
class AbstractFiniteMsgBodyHttpHandler<Input, void>:
    public detail::BaseFiniteMsgBodyHttpHandler<Input, void>
{
public:
    typedef std::function<void(
        const AuthorizationInfo& authzInfo,
        Input inputData,
        std::function<void(api::ResultCode resultCode)> completionHandler)> ExecuteRequestFunc;

    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc requestFunc)
        :
        detail::BaseFiniteMsgBodyHttpHandler<Input, void>(
            entityType,
            actionType,
            authorizationManager),
        m_requestFunc(std::move(requestFunc))
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer authInfo,
        Input inputData) override
    {
        //performing authorization
        //  authorization is performed here since it can depend on input data which 
        //  needs to be deserialized depending on request type
        if (!this->authorize(
                connection,
                request,
                authInfo,
                inputData,
                &authInfo))
            return;

        m_requestFunc(
            AuthorizationInfo(std::move(authInfo)),
            std::move(inputData),
            [this](api::ResultCode resultCode)
            {
                this->response()->headers.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(resultCode).toLatin1());
                this->requestCompleted(resultCodeToFusionRequestResult(resultCode));
            });
    }

private:
    ExecuteRequestFunc m_requestFunc;
};


/**
 * No input.
 */
template<typename Output>
class AbstractFiniteMsgBodyHttpHandler<void, Output>:
    public detail::BaseFiniteMsgBodyHttpHandler<void, Output>
{
public:
    typedef std::function<void(
        const AuthorizationInfo& authzInfo,
        std::function<void(api::ResultCode resultCode, Output outData)> completionHandler)> ExecuteRequestFunc;

    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc requestFunc)
        :
        detail::BaseFiniteMsgBodyHttpHandler<void, Output>(
            entityType,
            actionType,
            authorizationManager),
        m_requestFunc(std::move(requestFunc))
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer authInfo) override
    {
        if (!this->authorize(
                connection,
                request,
                authInfo,
                nx::utils::stree::ResourceContainer(),
                &authInfo))
            return;

        m_requestFunc(
            AuthorizationInfo(std::move(authInfo)),
            [this](api::ResultCode resultCode, Output outData)
            {
                this->response()->headers.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(resultCode).toLatin1());
                this->requestCompleted(
                    resultCodeToFusionRequestResult(resultCode),
                    std::move(outData));
            });
    }

private:
    ExecuteRequestFunc m_requestFunc;
};

/**
 * No input, no output.
 */
template<>
class AbstractFiniteMsgBodyHttpHandler<void, void>:
    public detail::BaseFiniteMsgBodyHttpHandler<void, void>
{
public:
    typedef std::function<void(
        const AuthorizationInfo& authzInfo,
        std::function<void(api::ResultCode resultCode)> completionHandler)> ExecuteRequestFunc;

    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc requestFunc)
        :
        detail::BaseFiniteMsgBodyHttpHandler<void, void>(
            entityType,
            actionType,
            authorizationManager),
        m_requestFunc(std::move(requestFunc))
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer authInfo) override
    {
        if (!this->authorize(
            connection,
            request,
            authInfo,
            nx::utils::stree::ResourceContainer(),
            &authInfo))
            return;

        m_requestFunc(
            AuthorizationInfo(std::move(authInfo)),
            [this](api::ResultCode resultCode)
            {
                this->response()->headers.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(resultCode).toLatin1());
                this->requestCompleted(resultCodeToFusionRequestResult(resultCode));
            });
    }

private:
    ExecuteRequestFunc m_requestFunc;
};

// TODO: #ak use variadic templates here to decrease number of specializations.

template<typename InputData>
class AbstractFreeMsgBodyHttpHandler:
    public detail::BaseFiniteMsgBodyHttpHandler<InputData, void>
{
public:
    typedef nx::utils::MoveOnlyFunc<void(
        nx_http::HttpServerConnection* const connection,
        const AuthorizationInfo& authzInfo,
        InputData inputData,
        nx::utils::MoveOnlyFunc<
        void(api::ResultCode, std::unique_ptr<nx_http::AbstractMsgBodySource>)>
            completionHandler)> ExecuteRequestFunc;

    AbstractFreeMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc requestFunc)
        :
        detail::BaseFiniteMsgBodyHttpHandler<InputData, void>(
            entityType,
            actionType,
            authorizationManager),
        m_requestFunc(std::move(requestFunc))
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer authInfo,
        InputData inputData) override
    {
        if (!this->authorize(
                connection,
                request,
                authInfo,
                nx::utils::stree::ResourceContainer(),
                &authInfo))
            return;

        m_requestFunc(
            connection,
            AuthorizationInfo(std::move(authInfo)),
            std::move(inputData),
            [this](
                api::ResultCode resultCode,
                std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody)
        {
            this->response()->headers.emplace(
                Qn::API_RESULT_CODE_HEADER_NAME,
                QnLexical::serialized(resultCode).toLatin1());

            if (resultCode == api::ResultCode::ok)
            {
                this->requestCompleted(
                    nx_http::StatusCode::ok,
                    std::move(responseMsgBody));
            }
            else
            {
                this->requestCompleted(
                    resultCodeToFusionRequestResult(resultCode));
            }
        });
    }

private:
    ExecuteRequestFunc m_requestFunc;
};

template<>
class AbstractFreeMsgBodyHttpHandler<void>:
    public detail::BaseFiniteMsgBodyHttpHandler<void, void>
{
public:
    typedef nx::utils::MoveOnlyFunc<void(
        nx_http::HttpServerConnection* connection,
        const AuthorizationInfo& authzInfo,
        nx::utils::MoveOnlyFunc<
        void(api::ResultCode, std::unique_ptr<nx_http::AbstractMsgBodySource>)
            > completionHandler)> ExecuteRequestFunc;

    AbstractFreeMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc requestFunc)
        :
        detail::BaseFiniteMsgBodyHttpHandler<void, void>(
            entityType,
            actionType,
            authorizationManager),
        m_requestFunc(std::move(requestFunc))
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        const nx_http::Request& request,
        nx::utils::stree::ResourceContainer authInfo) override
    {
        if (!this->authorize(
                connection,
                request,
                authInfo,
                nx::utils::stree::ResourceContainer(),
                &authInfo))
            return;

        m_requestFunc(
            connection,
            AuthorizationInfo(std::move(authInfo)),
            [this](
                api::ResultCode resultCode,
                std::unique_ptr<nx_http::AbstractMsgBodySource> responseMsgBody)
            {
                this->response()->headers.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(resultCode).toLatin1());

                if (resultCode == api::ResultCode::ok)
                    this->requestCompleted(
                        nx_http::StatusCode::ok,
                        std::move(responseMsgBody));
                else
                    this->requestCompleted(
                        resultCodeToFusionRequestResult(resultCode));
            });
    }

private:
    ExecuteRequestFunc m_requestFunc;
};

} // namespace cdb
} // namespace nx
