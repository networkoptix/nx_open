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
#include "access_control/types.h"
#include "managers/types.h"


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


//!Contains logic common for all cloud_db HTTP request handlers
template<typename Input = void, typename Output = void>
class AbstractFiniteMsgBodyHttpHandler
:
    public detail::BaseFiniteMsgBodyHttpHandler<Input, Output>
{
public:
    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager )
    :
        detail::BaseFiniteMsgBodyHttpHandler<Input, Output>(
            entityType,
            actionType,
            authorizationManager )
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

        processRequest(
            AuthorizationInfo( std::move( authInfo ) ),
            std::move( inputData ),
            std::bind(
                static_cast<void (nx_http::AbstractFusionRequestHandler<Input, Output>::*)(nx_http::StatusCode::Value, Output)>
                    (&nx_http::AbstractFusionRequestHandler<Input, Output>::requestCompleted),
                this,
                std::placeholders::_1,
                std::placeholders::_2 ) );
    }

protected:
    //!Implement request-specific logic in this function
    virtual void processRequest(
        AuthorizationInfo&& authzInfo,
        Input&& inputData,
        std::function<void(
            nx_http::StatusCode::Value statusCode,
            const Output& outputData )>&& completionHandler ) = 0;
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
    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager )
    :
        detail::BaseFiniteMsgBodyHttpHandler<Input, void>(
            entityType,
            actionType,
            authorizationManager )
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

        processRequest(
            AuthorizationInfo( std::move( authInfo ) ),
            std::move( inputData ),
            std::bind(
                &nx_http::AbstractFusionRequestHandler<Input, void>::requestCompleted,
                this,
                std::placeholders::_1 ) );
    }

protected:
    //!Implement request-specific logic in this function
    virtual void processRequest(
        AuthorizationInfo&& authzInfo,
        Input&& inputData,
        std::function<void( nx_http::StatusCode::Value statusCode )>&& completionHandler ) = 0;
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
    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager )
    :
        detail::BaseFiniteMsgBodyHttpHandler<void, Output>(
            entityType,
            actionType,
            authorizationManager )
    {
    }

    //!Implementation of AbstractFusionRequestHandler::processRequest
    virtual void processRequest(
        const nx_http::HttpServerConnection& /*connection*/,
        stree::ResourceContainer&& authInfo ) override
    {
        if( !this->authorize( authInfo, &authInfo ) )
            return;

        processRequest(
            AuthorizationInfo( std::move( authInfo ) ),
            std::bind(
                static_cast<void (nx_http::AbstractFusionRequestHandler<void, Output>::*)(nx_http::StatusCode::Value, Output)>
                    (&nx_http::AbstractFusionRequestHandler<void, Output>::requestCompleted),
                this,
                std::placeholders::_1,
                std::placeholders::_2 ) );
    }

protected:
    //!Implement request-specific logic in this function
    virtual void processRequest(
        AuthorizationInfo&& authzInfo,
        std::function<void( nx_http::StatusCode::Value, const Output& )>&& completionHandler ) = 0;
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
    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType,
        const AuthorizationManager& authorizationManager )
    :
        detail::BaseFiniteMsgBodyHttpHandler<void, void>(
            entityType,
            actionType,
            authorizationManager )
    {
    }

    //!Implementation of AbstractFusionRequestHandler::processRequest
    virtual void processRequest(
        const nx_http::HttpServerConnection& /*connection*/,
        stree::ResourceContainer&& authInfo ) override
    {
        if( !this->authorize( authInfo, &authInfo ) )
            return;

        processRequest(
            AuthorizationInfo( std::move( authInfo ) ),
            std::bind(
                static_cast<void (AbstractFusionRequestHandler<void, void>::*)(nx_http::StatusCode::Value)>
                    (&nx_http::AbstractFusionRequestHandler<void, void>::requestCompleted),
                this,
                std::placeholders::_1 ) );
    }

protected:
    //!Implement request-specific logic in this function
    virtual void processRequest(
        AuthorizationInfo&& authzInfo,
        std::function<void( nx_http::StatusCode::Value statusCode )>&& completionHandler ) = 0;
};



nx_http::StatusCode::Value resultCodeToHttpStatusCode( ResultCode resultCode );


}   //cdb
}   //nx

#endif  //cloud_db_base_http_handler_h
