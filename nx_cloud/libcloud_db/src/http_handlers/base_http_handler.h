/**********************************************************
* 19 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_base_http_handler_h
#define cloud_db_base_http_handler_h

#include <utils/common/cpp14.h>
#include <utils/network/http/buffer_source.h>
#include <utils/network/http/server/abstract_http_request_handler.h>
#include <utils/network/http/server/http_server_connection.h>

#include "access_control/authorization_manager.h"
#include "access_control/types.h"
#include "managers/types.h"


namespace nx {
namespace cdb {


//!Contains logic common for all cloud_db HTTP request handlers
template<typename InputData, typename OutputData = void>
class AbstractFiniteMsgBodyHttpHandler
:
    public nx_http::AbstractHttpRequestHandler
{
public:
    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType )
    :
        m_entityType( entityType ),
        m_actionType( actionType )
    {
    }

    virtual ~AbstractFiniteMsgBodyHttpHandler()
    {
    }

    //!Implementation of AbstractHttpRequestHandler::processRequest
    /*!
        \warning this object is allowed to be removed from within \a completionHandler call!
    */
    virtual void processRequest(
        const nx_http::HttpServerConnection& /*connection*/,
        stree::ResourceContainer&& authInfo,
        const nx_http::Request& /*request*/,
        nx_http::Response* const response,
        std::function<void(
            nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler ) override
    {
        m_completionHandler = std::move( completionHandler );

        InputData inputData;
        //TODO #ak deserializing inputData

        //TODO #ak performing authorization
        //  authorization is performed here since it can depend on input data which 
        //  needs to be deserialized depending on request type
        if( !AuthorizationManager::instance()->authorize(
                stree::MultiSourceResourceReader( authInfo, inputData ),
                m_entityType,
                m_actionType,
                &authInfo ) )   //using same object since we can move it
        {
            requestCompleted( nx_http::StatusCode::unauthorized, OutputData() );
            return;
        }

        processRequest(
            AuthorizationInfo( std::move(authInfo) ),
            std::move(inputData),
            response,
            std::bind(
                &AbstractFiniteMsgBodyHttpHandler<InputData, OutputData>::requestCompleted,
                this,
                std::placeholders::_1,
                std::placeholders::_2 ) );
    }

protected:
    //!Implement request-specific logic in this function
    virtual void processRequest(
        AuthorizationInfo&& authzInfo,
        InputData&& inputData,
        //const stree::AbstractResourceReader& inputParams,
        nx_http::Response* const response,
        std::function<void(
            nx_http::StatusCode::Value statusCode,
            const OutputData& outputData )>&& completionHandler ) = 0;

private:
    const EntityType m_entityType;
    const DataActionType m_actionType;
    std::function<void(
        nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )> m_completionHandler;

    void requestCompleted(
        nx_http::StatusCode::Value statusCode,
        const OutputData& /*outputData*/ )
    {
        //TODO #ak serializing outputData into message body buffer using proper format

        auto completionHandler = std::move(m_completionHandler);
        completionHandler(
            statusCode,
            std::make_unique<nx_http::BufferSource>(
                "text/html",
                "<html>"
                    "<h1>Hello</h1>"
                    "<body>Hello from base cloud_db handler with output data</body>"
                "</html>\n" ) );
    }
};


//TODO #ak remove copy-paste (partial specialization)

template<typename InputData>
class AbstractFiniteMsgBodyHttpHandler<InputData, void>
:
    public nx_http::AbstractHttpRequestHandler
{
public:
    AbstractFiniteMsgBodyHttpHandler(
        EntityType entityType,
        DataActionType actionType )
    :
        m_entityType( entityType ),
        m_actionType( actionType )
    {
    }

    virtual ~AbstractFiniteMsgBodyHttpHandler()
    {
    }

    //!Implementation of AbstractHttpRequestHandler::processRequest
    /*!
        \warning this object is allowed to be removed from within \a completionHandler call!
    */
    virtual void processRequest(
        const nx_http::HttpServerConnection& /*connection*/,
        stree::ResourceContainer&& authInfo,
        const nx_http::Request& /*request*/,
        nx_http::Response* const response,
        std::function<void(
            nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler ) override
    {
        m_completionHandler = std::move( completionHandler );

        InputData inputData;
        //TODO #ak deserializing inputData

        //TODO #ak performing authorization
        //  authorization is performed here since it can depend on input data which 
        //  needs to be deserialized depending on request type
        if( !AuthorizationManager::instance()->authorize(
                stree::MultiSourceResourceReader( authInfo, inputData ),
                m_entityType,
                m_actionType,
                &authInfo ) )   //using same object since we can move it
        {
            requestCompleted( nx_http::StatusCode::unauthorized );
            return;
        }

        processRequest(
            AuthorizationInfo( std::move(authInfo) ),
            std::move(inputData),
            response,
            std::bind(
                &AbstractFiniteMsgBodyHttpHandler<InputData>::requestCompleted,
                this,
                std::placeholders::_1 ) );
    }

protected:
    //!Implement request-specific logic in this function
    virtual void processRequest(
        AuthorizationInfo&& authzInfo,
        InputData&& inputData,
        //const stree::AbstractResourceReader& inputParams,
        nx_http::Response* const response,
        std::function<void( nx_http::StatusCode::Value statusCode )>&& completionHandler ) = 0;

private:
    const EntityType m_entityType;
    const DataActionType m_actionType;
    std::function<void(
        const nx_http::StatusCode::Value statusCode,
        std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )> m_completionHandler;

    void requestCompleted( nx_http::StatusCode::Value statusCode )
    {
        auto completionHandler = std::move(m_completionHandler);
        completionHandler(
            statusCode,
            std::make_unique<nx_http::BufferSource>(
                "text/html",
                "<html>"
                    "<h1>Hello</h1>"
                    "<body>Hello from base cloud_db handler w/o output data</body>"
                "</html>\n" ) );
    }
};

nx_http::StatusCode::Value resultCodeToHttpStatusCode( ResultCode resultCode );


}   //cdb
}   //nx

#endif  //cloud_db_base_http_handler_h
