#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>

#include "../http_types.h"

namespace nx_http {

enum class FusionRequestErrorClass
{
    noError = 0,
    badRequest,
    unauthorized,
    logicError,
    ioError,
    //!indicates server bug?
    internalError
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(FusionRequestErrorClass)

enum class FusionRequestErrorDetail
{
    ok = 0,
    responseSerializationError,
    deserializationError,
    notAcceptable
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(FusionRequestErrorDetail)

class NX_NETWORK_API FusionRequestResult
{
public:
    FusionRequestErrorClass errorClass;
    QString resultCode;
    int errorDetail;
    QString errorText;

    /** Creates OK result */
    FusionRequestResult();
    FusionRequestResult(
        FusionRequestErrorClass _errorClass,
        QString _resultCode,
        int _errorDetail,
        QString _errorText);
    FusionRequestResult(
        FusionRequestErrorClass _errorClass,
        QString _resultCode,
        FusionRequestErrorDetail _errorDetail,
        QString _errorText);

    nx_http::StatusCode::Value httpStatusCode() const;
    void setHttpStatusCode(nx_http::StatusCode::Value statusCode);

private:
    boost::optional<nx_http::StatusCode::Value> m_httpStatusCode;

    nx_http::StatusCode::Value calculateHttpStatusCode() const;
};

#define FusionRequestResult_Fields (errorClass)(resultCode)(errorDetail)(errorText)

QN_FUSION_DECLARE_FUNCTIONS(nx_http::FusionRequestErrorDetail, (lexical), NX_NETWORK_API)
QN_FUSION_DECLARE_FUNCTIONS(nx_http::FusionRequestErrorClass, (lexical), NX_NETWORK_API)
QN_FUSION_DECLARE_FUNCTIONS(nx_http::FusionRequestResult, (json), NX_NETWORK_API)

} // namespace nx_http
