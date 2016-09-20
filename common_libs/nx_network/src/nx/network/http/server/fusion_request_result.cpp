/**********************************************************
* Nov 20, 2015
* a.kolesnikov
***********************************************************/

#include "fusion_request_result.h"

#include <nx/fusion/model_functions.h>


namespace nx_http {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(FusionRequestErrorClass,
    (FusionRequestErrorClass::noError, "noError")
    (FusionRequestErrorClass::badRequest, "badRequest")
    (FusionRequestErrorClass::unauthorized, "unauthorized")
    (FusionRequestErrorClass::logicError, "logicError")
    (FusionRequestErrorClass::ioError, "ioError")
    (FusionRequestErrorClass::internalError, "internalError")
    )


QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(FusionRequestErrorDetail,
    (FusionRequestErrorDetail::ok, "ok")
    (FusionRequestErrorDetail::responseSerializationError, "responseSerializationError")
    (FusionRequestErrorDetail::deserializationError, "deserializationError")
    (FusionRequestErrorDetail::notAcceptable, "notAcceptable")
    )


FusionRequestResult::FusionRequestResult()
:
    errorClass(FusionRequestErrorClass::noError),
    resultCode(QnLexical::serialized(FusionRequestErrorDetail::ok)),
    errorDetail(static_cast<int>(FusionRequestErrorDetail::ok))
{
}

FusionRequestResult::FusionRequestResult(
    FusionRequestErrorClass _errorClass,
    QString _resultCode,
    int _errorDetail,
    QString _errorText)
:
    errorClass(_errorClass),
    resultCode(_resultCode),
    errorDetail(_errorDetail),
    errorText(std::move(_errorText))
{
}

FusionRequestResult::FusionRequestResult(
    FusionRequestErrorClass _errorClass,
    QString _resultCode,
    FusionRequestErrorDetail _errorDetail,
    QString _errorText)
:
    errorClass(_errorClass),
    resultCode(_resultCode),
    errorDetail(static_cast<int>(_errorDetail)),
    errorText(std::move(_errorText))
{
}

nx_http::StatusCode::Value FusionRequestResult::httpStatusCode() const
{
    switch (errorClass)
    {
        case FusionRequestErrorClass::noError:
            return nx_http::StatusCode::ok;
        case FusionRequestErrorClass::badRequest:
            switch (static_cast<FusionRequestErrorDetail>(errorDetail))
            {
                case FusionRequestErrorDetail::notAcceptable:
                    return nx_http::StatusCode::notAcceptable;
                default:
                    return nx_http::StatusCode::badRequest;
            }
        case FusionRequestErrorClass::unauthorized:
            // This is authorization failure, not authentication! 
                // "401 Unauthorized" is not applicable here since it 
                // actually signals authentication error.
            return nx_http::StatusCode::notFound;
        case FusionRequestErrorClass::logicError:
            // Using "404 Not Found" to signal any logic error. 
                // It is allowed by HTTP. See [rfc2616, 10.4.5] for details
            return nx_http::StatusCode::notFound;
        case FusionRequestErrorClass::ioError:
            return nx_http::StatusCode::serviceUnavailable;
        case FusionRequestErrorClass::internalError:
            return nx_http::StatusCode::internalServerError;
        default:
            NX_ASSERT(false);
            return nx_http::StatusCode::internalServerError;
    }
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (FusionRequestResult),
    (json),
    _Fields)

}
