/**********************************************************
* Sep 4, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CLIENT_DATA_TYPES_H
#define NX_CDB_CLIENT_DATA_TYPES_H

#include <utils/common/model_functions_fwd.h>
#include <utils/network/http/httptypes.h>

#include <cdb/result_code.h>


namespace nx {
namespace cdb {
namespace api {

nx_http::StatusCode::Value resultCodeToHttpStatusCode(ResultCode resultCode);
ResultCode httpStatusCodeToResultCode(nx_http::StatusCode::Value statusCode);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ResultCode), (lexical))


}   //api
}   //cdb
}   //nx

#endif //NX_CDB_CLIENT_DATA_TYPES_H
