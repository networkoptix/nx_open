/**********************************************************
* 19 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_base_http_handler_h
#define cloud_db_base_http_handler_h

#include <utils/common/cpp14.h>
#include <utils/network/http/buffer_source.h>
#include <utils/network/http/server/abstract_fusion_request_handler.h>
#include <utils/network/http/server/abstract_http_request_handler.h>
#include <utils/network/http/server/http_server_connection.h>

#include "access_control/authorization_manager.h"
#include "access_control/auth_types.h"
#include "managers/managers_types.h"


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
        const stree::AbstractResourceReader& dataToAuthorize,
        stree::AbstractResourceWriter* const authzInfo )
    {
        //performing authorization
        //  authorization is performed here since it can depend on input data which 
        //  needs to be deserialized depending on request type
        if( !m_authorizationManager.authorize(
                dataToAuthorize,
                this->m_entityType,
                this->m_actionType,
                authzInfo ) )   //using same object since we can move it
        {
            this->requestCompleted( nx_http::StatusCode::unauthorized );
            return false;
        }
        return true;
    }
};

}   //detail


nx_http::StatusCode::Value resultCodeToHttpStatusCode(api::ResultCode resultCode );


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
        ExecuteRequestFunc&& requestFunc )
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
        const nx_http::HttpServerConnection& /*connection*/,
        stree::ResourceContainer&& authInfo,
        Input inputData ) override
    {
        if( !this->authorize(
                stree::MultiSourceResourceReader( authInfo, inputData ),
                &authInfo ) )
            return;

        m_requestFunc(
            AuthorizationInfo( std::move( authInfo ) ),
            std::move( inputData ),
            [this]( api::ResultCode resultCode, Output outData ) {
                this->requestCompleted( resultCodeToHttpStatusCode( resultCode ), std::move(outData) );
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
        std::function<void( api::ResultCode resultCode )>&& completionHandler )> ExecuteRequestFunc;

    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc&& requestFunc )
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
        const nx_http::HttpServerConnection& /*connection*/,
        stree::ResourceContainer&& authInfo,
        Input inputData ) override
    {
        //performing authorization
        //  authorization is performed here since it can depend on input data which 
        //  needs to be deserialized depending on request type
        if( !this->authorize(
                stree::MultiSourceResourceReader( authInfo, inputData ),
                &authInfo ) )
            return;

        m_requestFunc(
            AuthorizationInfo( std::move( authInfo ) ),
            std::move( inputData ),
            [this]( api::ResultCode resultCode ) {
                this->requestCompleted( resultCodeToHttpStatusCode( resultCode ) );
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
        std::function<void( api::ResultCode resultCode, Output outData )>&& completionHandler )> ExecuteRequestFunc;

    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc&& requestFunc )
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
        const nx_http::HttpServerConnection& /*connection*/,
        stree::ResourceContainer&& authInfo ) override
    {
        if( !this->authorize( authInfo, &authInfo ) )
            return;

        m_requestFunc(
            AuthorizationInfo( std::move( authInfo ) ),
            [this]( api::ResultCode resultCode, Output outData ) {
                this->requestCompleted( resultCodeToHttpStatusCode( resultCode ), std::move( outData ) );
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
        std::function<void( api::ResultCode resultCode )>&& completionHandler )> ExecuteRequestFunc;

    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager,
        ExecuteRequestFunc&& requestFunc )
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
        const nx_http::HttpServerConnection& /*connection*/,
        stree::ResourceContainer&& authInfo ) override
    {
        if( !this->authorize( authInfo, &authInfo ) )
            return;

        m_requestFunc(
            AuthorizationInfo( std::move( authInfo ) ),
            [this]( api::ResultCode resultCode ) {
                this->requestCompleted( resultCodeToHttpStatusCode( resultCode ) );
            } );
    }

private:
    ExecuteRequestFunc m_requestFunc;
};


}   //cdb
}   //nx

#endif  //cloud_db_base_http_handler_h
