#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/http_types.h>
#include <nx/network/http/server/fusion_request_result.h>

#include <nx/cloud/cdb/api/result_code.h>

namespace nx {
namespace cdb {
namespace api {

nx::network::http::StatusCode::Value resultCodeToHttpStatusCode(ResultCode resultCode);
ResultCode httpStatusCodeToResultCode(nx::network::http::StatusCode::Value statusCode);

nx::network::http::FusionRequestResult resultCodeToFusionRequestResult(ResultCode resultCode);
ResultCode fusionRequestResultToResultCode(nx::network::http::FusionRequestResult result);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(ResultCode)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((ResultCode), (lexical))

} // namespace api
} // namespace cdb
} // namespace nx
