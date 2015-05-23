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
        const nx_http::Request& /*request*/ )
    {
        //TODO #ak performing authorization and invoking specific handler

        requestDone(
            nx_http::StatusCode::ok,
            std::make_unique<nx_http::BufferSource>(
                "text/html",
                "<html><h1>Hello from base cloud_db handler</h1></html>\n" ) );
    }
}
