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

#include "access_control/types.h"


namespace cdb
{
    //!Contains logic common for all cloud_db HTTP request handlers
    template<typename InputData, typename OutputData>
    class AbstractFiniteMsgBodyHttpHandler
    :
        public nx_http::AbstractHttpRequestHandler
    {
    public:
        virtual ~AbstractFiniteMsgBodyHttpHandler()
        {
            int x = 0;
        }

        //!Implementation of AbstractHttpRequestHandler::processRequest
        virtual void processRequest(
            const nx_http::HttpServerConnection& /*connection*/,
            const nx_http::Request& /*request*/,
            nx_http::Response* const response,
            std::function<void(
                const nx_http::StatusCode::Value statusCode,
                std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler ) override
        {
            //TODO #ak performing authorization and invoking specific handler
            AuthorizationInfo authzInfo;
            //stree::ResourceContainer res;
            InputData inputData;
            //TODO #ak deserializing inputData

            m_completionHandler = std::move(completionHandler);
            processRequest(
                authzInfo,
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
            const AuthorizationInfo& authzInfo,
            const InputData& inputData,
            //const stree::AbstractResourceReader& inputParams,
            nx_http::Response* const response,
            std::function<void(
                const nx_http::StatusCode::Value statusCode,
                const OutputData& outputData )>&& completionHandler ) = 0;

    private:
        void requestCompleted(
            const nx_http::StatusCode::Value statusCode,
            const OutputData& /*outputData*/ )
        {
            //TODO #ak serializing response into message body buffer

            m_completionHandler(
                statusCode,
                std::make_unique<nx_http::BufferSource>(
                    "text/html",
                    "<html>"
                        "<h1>Hello</h1>"
                        "<body>Hello from base cloud_db add_account handler</body>"
                    "</html>\n" ) );
        }

        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )> m_completionHandler;
    };
}

#endif  //cloud_db_base_http_handler_h
