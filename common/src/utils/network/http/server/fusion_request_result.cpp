/**********************************************************
* Nov 20, 2015
* a.kolesnikov
***********************************************************/

#include "fusion_request_result.h"

#include <utils/common/model_functions.h>


namespace nx_http {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(FusionRequestErrorClass,
    (FusionRequestErrorClass::noError, "noError")
    (FusionRequestErrorClass::badRequest, "badRequest")
    (FusionRequestErrorClass::unauthorized, "unauthorized")
    (FusionRequestErrorClass::ioError, "ioError")
    (FusionRequestErrorClass::internalError, "internalError")
    )


FusionRequestResult::FusionRequestResult()
:
    resultCode(FusionRequestErrorClass::noError),
    errorDetail(ecNoDetail)
{
}

FusionRequestResult::FusionRequestResult(
    FusionRequestErrorClass _errorClass,
    int _errorDetail,
    QString _errorText)
:
    resultCode(_errorClass),
    errorDetail(_errorDetail),
    errorText(std::move(_errorText))
{
}

nx_http::StatusCode::Value FusionRequestResult::httpStatusCode() const
{
    switch (resultCode)
    {
        case FusionRequestErrorClass::noError:
            return nx_http::StatusCode::ok;
        case FusionRequestErrorClass::badRequest:
            return nx_http::StatusCode::badRequest;
        case FusionRequestErrorClass::unauthorized:
            return nx_http::StatusCode::ok;
        case FusionRequestErrorClass::logicError:
            return nx_http::StatusCode::ok;
        case FusionRequestErrorClass::ioError:
            return nx_http::StatusCode::serviceUnavailable;
        case FusionRequestErrorClass::internalError:
            return nx_http::StatusCode::internalServerError;
        default:
            assert(false);
            return nx_http::StatusCode::internalServerError;
    }
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (FusionRequestResult),
    (json),
    _Fields)

}
