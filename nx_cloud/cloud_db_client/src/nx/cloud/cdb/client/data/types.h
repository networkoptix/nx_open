/**********************************************************
* Sep 4, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CLIENT_DATA_TYPES_H
#define NX_CDB_CLIENT_DATA_TYPES_H

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/server/fusion_request_result.h>

#include <nx/cloud/cdb/api/result_code.h>


namespace nx {
namespace cdb {
namespace api {

nx_http::StatusCode::Value resultCodeToHttpStatusCode(ResultCode resultCode);
ResultCode httpStatusCodeToResultCode(nx_http::StatusCode::Value statusCode);

nx_http::FusionRequestResult resultCodeToFusionRequestResult(ResultCode resultCode);
ResultCode fusionRequestResultToResultCode(nx_http::FusionRequestResult result);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ResultCode), (lexical))

}   //api
}   //cdb
}   //nx

#endif //NX_CDB_CLIENT_DATA_TYPES_H
