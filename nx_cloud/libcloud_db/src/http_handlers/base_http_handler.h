/**********************************************************
* 19 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_base_http_handler_h
#define cloud_db_base_http_handler_h

#include <http/custom_headers.h>
#include <utils/common/cpp14.h>
#include <utils/network/http/buffer_source.h>
#include <utils/network/http/server/abstract_fusion_request_handler.h>
#include <utils/network/http/server/abstract_http_request_handler.h>
#include <utils/network/http/server/http_server_connection.h>

#include <cloud_db_client/src/data/types.h>
#include <utils/common/model_functions.h>

#include "access_control/authorization_manager.h"
#include "access_control/auth_types.h"
#include "managers/managers_types.h"
#include "stree/http_request_attr_reader.h"
#include "stree/socket_attr_reader.h"


namespace nx {
namespace cdb {
namespace detail {

template<typename Input, typename Output>
class BaseFiniteMsgBodyHttpHandler
:
    public nx_http::AbstractFusionRequestHandler<Input, Output>
{
public:
    BaseFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager )
    :
        m_entityType( entityType ),
        m_actionType( actionType ),
        m_authorizationManager( authorizationManager )
    {
    }

protected:
    const EntityType m_entityType;
    const DataActionType m_actionType;
    const AuthorizationManager& m_authorizationManager;

    bool authorize(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        const stree::AbstractResourceReader& authenticationData,
        const stree::AbstractResourceReader& dataToAuthorize,
        stree::ResourceContainer* const authzInfo )
    {
        SocketResourceReader socketResources(*connection.socket());
        HttpRequestResourceReader httpRequestResources(request);

        //performing authorization
        //  authorization is performed here since it can depend on input data which 
        //  needs to be deserialized depending on request type
        if( !m_authorizationManager.authorize(
                authenticationData,
                stree::MultiSourceResourceReader(
                    socketResources,
                    httpRequestResources,
                    dataToAuthorize),
                this->m_entityType,
                this->m_actionType,
                authzInfo ) )   //using same object since we can move it
        {
            nx_http::FusionRequestResult result(
                nx_http::FusionRequestErrorClass::unauthorized,
                nx_http::FusionRequestResult::ecNoDetail,
                QString());
            this->response()->headers.emplace(
                Qn::API_RESULT_CODE_HEADER_NAME,
                QnLexical::serialized(api::ResultCode::forbidden).toLatin1());
            this->requestCompleted(std::move(result));
            return false;
        }
        return true;
    }
};

}   //detail


//!Contains logic common for all cloud_db HTTP request handlers
template<typename Input = void, typename Output = void>
class AbstractFiniteMsgBodyHttpHandler
:
    public detail::BaseFiniteMsgBodyHttpHandler<Input, Output>
{
public:
    typedef std::function<void(
        const AuthorizationInfo& authzInfo,
        Input inputData,
        std::function<void( api::ResultCode resultCode, Output outData )>&& completionHandler )> ExecuteRequestFunc;

    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc requestFunc )
    :
        detail::BaseFiniteMsgBodyHttpHandler<Input, Output>(
            entityType,
            actionType,
            authorizationManager ),
        m_requestFunc( std::move( requestFunc ) )
    {
    }

    //!Implementation of AbstractFusionRequestHandler::processRequest
    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        stree::ResourceContainer authInfo,
        Input inputData ) override
    {
        if( !this->authorize(
                connection,
                request,
                authInfo,
                inputData,
                &authInfo ) )
            return;

        m_requestFunc(
            AuthorizationInfo( std::move( authInfo ) ),
            std::move( inputData ),
            [this]( api::ResultCode resultCode, Output outData ) {
                this->response()->headers.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(resultCode).toLatin1());
                this->requestCompleted(
                    resultCodeToFusionRequestResult(resultCode),
                    std::move(outData));
            } );
    }

private:
    ExecuteRequestFunc m_requestFunc;
};


/*!
    No output
*/
template<typename Input>
class AbstractFiniteMsgBodyHttpHandler<Input, void>
:
    public detail::BaseFiniteMsgBodyHttpHandler<Input, void>
{
public:
    typedef std::function<void(
        const AuthorizationInfo& authzInfo,
        Input inputData,
        std::function<void( api::ResultCode resultCode )> completionHandler )> ExecuteRequestFunc;

    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc requestFunc )
    :
        detail::BaseFiniteMsgBodyHttpHandler<Input, void>(
            entityType,
            actionType,
            authorizationManager ),
        m_requestFunc( std::move(requestFunc) )
    {
    }

    //!Implementation of AbstractFusionRequestHandler::processRequest
    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        stree::ResourceContainer authInfo,
        Input inputData ) override
    {
        //performing authorization
        //  authorization is performed here since it can depend on input data which 
        //  needs to be deserialized depending on request type
        if( !this->authorize(
                connection,
                request,
                authInfo,
                inputData,
                &authInfo ) )
            return;

        m_requestFunc(
            AuthorizationInfo( std::move( authInfo ) ),
            std::move( inputData ),
            [this]( api::ResultCode resultCode ) {
                this->response()->headers.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(resultCode).toLatin1());
                this->requestCompleted(resultCodeToFusionRequestResult(resultCode));
            } );
    }

private:
    ExecuteRequestFunc m_requestFunc;
};


/*!
    No input
*/
template<typename Output>
class AbstractFiniteMsgBodyHttpHandler<void, Output>
:
    public detail::BaseFiniteMsgBodyHttpHandler<void, Output>
{
public:
    typedef std::function<void(
        const AuthorizationInfo& authzInfo,
        std::function<void( api::ResultCode resultCode, Output outData )> completionHandler )> ExecuteRequestFunc;

    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc requestFunc )
    :
        detail::BaseFiniteMsgBodyHttpHandler<void, Output>(
            entityType,
            actionType,
            authorizationManager ),
        m_requestFunc( std::move( requestFunc ) )
    {
    }

    //!Implementation of AbstractFusionRequestHandler::processRequest
    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        stree::ResourceContainer authInfo ) override
    {
        if (!this->authorize(
                connection,
                request,
                authInfo,
                stree::ResourceContainer(),
                &authInfo))
            return;

        m_requestFunc(
            AuthorizationInfo( std::move( authInfo ) ),
            [this]( api::ResultCode resultCode, Output outData ) {
                this->response()->headers.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(resultCode).toLatin1());
                this->requestCompleted(
                    resultCodeToFusionRequestResult(resultCode),
                    std::move(outData));
            } );
    }

private:
    ExecuteRequestFunc m_requestFunc;
};


/*!
    No input, no output
*/
template<>
class AbstractFiniteMsgBodyHttpHandler<void, void>
:
    public detail::BaseFiniteMsgBodyHttpHandler<void, void>
{
public:
    typedef std::function<void(
        const AuthorizationInfo& authzInfo,
        std::function<void( api::ResultCode resultCode )> completionHandler )> ExecuteRequestFunc;

    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc requestFunc )
    :
        detail::BaseFiniteMsgBodyHttpHandler<void, void>(
            entityType,
            actionType,
            authorizationManager ),
        m_requestFunc( std::move( requestFunc ) )
    {
    }

    //!Implementation of AbstractFusionRequestHandler::processRequest
    virtual void processRequest(
        const nx_http::HttpServerConnection& connection,
        const nx_http::Request& request,
        stree::ResourceContainer authInfo ) override
    {
        if (!this->authorize(
                connection,
                request,
                authInfo,
                stree::ResourceContainer(),
                &authInfo))
            return;

        m_requestFunc(
            AuthorizationInfo( std::move( authInfo ) ),
            [this]( api::ResultCode resultCode ) {
                this->response()->headers.emplace(
                    Qn::API_RESULT_CODE_HEADER_NAME,
                    QnLexical::serialized(resultCode).toLatin1());
                this->requestCompleted(resultCodeToFusionRequestResult(resultCode));
            } );
    }

private:
    ExecuteRequestFunc m_requestFunc;
};

}   //cdb
}   //nx

#endif  //cloud_db_base_http_handler_h
