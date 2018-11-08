#include "fusion_rest_handler.h"

namespace QnFusionRestHandlerDetail {

Qn::SerializationFormat formatFromParams(const QnRequestParamList& params)
{
    Qn::SerializationFormat format = Qn::JsonFormat;
    QnLexical::deserialize(params.value(lit("format")), &format);
    return format;
}

} // namespace QnFusionRestHandlerDetail

int QnFusionRestHandler::executePost(
    const QString& /*path*/, const QnRequestParamList& /*params*/, const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/, QByteArray& /*result*/,
    QByteArray& /*resultContentType*/, const QnRestConnectionProcessor* /*processor*/)
{
    return nx::network::http::StatusCode::notImplemented;
}

int QnFusionRestHandler::executePut(
    const QString& /*path*/, const QnRequestParamList& /*params*/, const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/, QByteArray& /*result*/,
    QByteArray& /*resultContentType*/, const QnRestConnectionProcessor* /*processor*/)
{
    return nx::network::http::StatusCode::notImplemented;
}

int QnFusionRestHandler::executeGet(
    const QString& /*path*/, const QnRequestParamList& /*params*/, QByteArray& /*result*/,
    QByteArray& /*contentType*/, const QnRestConnectionProcessor* /*processor*/)
{
    return nx::network::http::StatusCode::notImplemented;
}

int QnFusionRestHandler::executeDelete(
    const QString& /*path*/, const QnRequestParamList& /*params*/, QByteArray& /*result*/,
    QByteArray& /*contentType*/, const QnRestConnectionProcessor* /*processor*/)
{
    return nx::network::http::StatusCode::notImplemented;
}

int QnFusionRestHandler::makeError(
    int httpStatusCode, const QString &errorMessage, QByteArray* outBody,
    QByteArray* outContentType, Qn::SerializationFormat format, bool extraFormatting,
    QnRestResult::Error error)
{
    QnRestResult restResult;
    restResult.setError(error, errorMessage);
    QnFusionRestHandlerDetail::serialize(
        restResult, *outBody, *outContentType, format, extraFormatting);
    return httpStatusCode;
}
