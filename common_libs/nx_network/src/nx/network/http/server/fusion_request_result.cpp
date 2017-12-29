#include "fusion_request_result.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace network {
namespace http {

FusionRequestResult::FusionRequestResult():
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

void FusionRequestResult::setHttpStatusCode(
    nx::network::http::StatusCode::Value statusCode)
{
    m_httpStatusCode = statusCode;
}

nx::network::http::StatusCode::Value FusionRequestResult::httpStatusCode() const
{
    if (m_httpStatusCode)
        return *m_httpStatusCode;
    else
        return calculateHttpStatusCode();
}

nx::network::http::StatusCode::Value FusionRequestResult::calculateHttpStatusCode() const
{
    switch (errorClass)
    {
        case FusionRequestErrorClass::noError:
            return nx::network::http::StatusCode::ok;

        case FusionRequestErrorClass::badRequest:
            switch (static_cast<FusionRequestErrorDetail>(errorDetail))
            {
                case FusionRequestErrorDetail::notAcceptable:
                    return nx::network::http::StatusCode::notAcceptable;
                default:
                    return nx::network::http::StatusCode::badRequest;
            }

        case FusionRequestErrorClass::unauthorized:
            // This is authorization failure, not authentication!
                // "401 Unauthorized" is not applicable here since it
                // actually signals authentication error.
            return nx::network::http::StatusCode::forbidden;

        case FusionRequestErrorClass::logicError:
            // Using "404 Not Found" to signal any logic error.
                // It is allowed by HTTP. See [rfc2616, 10.4.5] for details
            return nx::network::http::StatusCode::notFound;

        case FusionRequestErrorClass::ioError:
            return nx::network::http::StatusCode::serviceUnavailable;

        case FusionRequestErrorClass::internalError:
            return nx::network::http::StatusCode::internalServerError;

        default:
            NX_ASSERT(false);
            return nx::network::http::StatusCode::internalServerError;
    }
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (FusionRequestResult),
    (json),
    _Fields)


QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::network::http, FusionRequestErrorClass,
    (nx::network::http::FusionRequestErrorClass::noError, "noError")
    (nx::network::http::FusionRequestErrorClass::badRequest, "badRequest")
    (nx::network::http::FusionRequestErrorClass::unauthorized, "unauthorized")
    (nx::network::http::FusionRequestErrorClass::logicError, "logicError")
    (nx::network::http::FusionRequestErrorClass::ioError, "ioError")
    (nx::network::http::FusionRequestErrorClass::internalError, "internalError")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::network::http, FusionRequestErrorDetail,
    (nx::network::http::FusionRequestErrorDetail::ok, "ok")
    (nx::network::http::FusionRequestErrorDetail::responseSerializationError, "responseSerializationError")
    (nx::network::http::FusionRequestErrorDetail::deserializationError, "deserializationError")
    (nx::network::http::FusionRequestErrorDetail::notAcceptable, "notAcceptable")
)

} // namespace nx
} // namespace network
} // namespace http
