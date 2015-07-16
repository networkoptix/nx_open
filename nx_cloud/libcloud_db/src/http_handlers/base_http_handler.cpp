/**********************************************************
* 19 may 2015
* a.kolesnikov
***********************************************************/

#include "base_http_handler.h"

#include <memory>

#include <utils/common/cpp14.h>
#include <utils/network/http/buffer_source.h>


namespace cdb_api
{
    void BaseHttpHandler::processRequest(
        const nx_http::HttpServerConnection& /*connection*/,
        const nx_http::Request& /*request*/,
        nx_http::Response* const response,
        std::function<void(
            const nx_http::StatusCode::Value statusCode,
            std::unique_ptr<nx_http::AbstractMsgBodySource> dataSource )>&& completionHandler )
    {
        //TODO #ak performing authorization and invoking specific handler
        AuthorizationInfo authzInfo;
        stree::ResourceContainer res;

        processRequest( authzInfo, res, response, std::move( completionHandler ) );
    }
}
