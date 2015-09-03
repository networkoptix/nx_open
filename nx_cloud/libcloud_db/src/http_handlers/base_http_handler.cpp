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


nx_http::StatusCode::Value resultCodeToHttpStatusCode(api::ResultCode resultCode)
{
    switch( resultCode )
    {
        case api::ResultCode::ok:
            return nx_http::StatusCode::ok;
        case api::ResultCode::notAuthorized:
            return nx_http::StatusCode::unauthorized;
        case api::ResultCode::forbidden:
            return nx_http::StatusCode::forbidden;
        case api::ResultCode::notFound:
            return nx_http::StatusCode::notFound;
        case api::ResultCode::alreadyExists:
            return nx_http::StatusCode::forbidden;
        case api::ResultCode::dbError:
            return nx_http::StatusCode::internalServerError;
    }
    return nx_http::StatusCode::internalServerError;
}


}   //cdb
}   //nx
