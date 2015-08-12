/**********************************************************
* 19 may 2015
* a.kolesnikov
***********************************************************/

#include "base_http_handler.h"

#include <memory>

#include <utils/common/cpp14.h>
#include <utils/network/http/buffer_source.h>


namespace nx {
namespace cdb {


nx_http::StatusCode::Value resultCodeToHttpStatusCode( ResultCode resultCode )
{
    switch( resultCode )
    {
        case ResultCode::ok:
            return nx_http::StatusCode::ok;
        case ResultCode::notAuthorized:
            return nx_http::StatusCode::unauthorized;
        case ResultCode::notFound:
            return nx_http::StatusCode::notFound;
        case ResultCode::alreadyExists:
            return nx_http::StatusCode::forbidden;
        case ResultCode::dbError:
            return nx_http::StatusCode::internalServerError;
        default:
            return nx_http::StatusCode::internalServerError;
    }
}


}   //cdb
}   //nx
