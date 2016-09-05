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
    return nx_http::StatusCode::notImplemented;
}

int QnFusionRestHandler::executeGet(
    const QString& /*path*/, const QnRequestParamList& /*params*/, QByteArray& /*result*/,
    QByteArray& /*contentType*/, const QnRestConnectionProcessor* /*processor*/)
{
    return nx_http::StatusCode::notImplemented;
}

int QnFusionRestHandler::genericError(
    int errCode, const QString &error, QByteArray& result, QByteArray& contentType,
    Qn::SerializationFormat format, bool extraFormatting )
{
    QnRestResult restResult;
    restResult.setError(QnRestResult::CantProcessRequest, error);
    QnFusionRestHandlerDetail::serialize(restResult, result, contentType, format, extraFormatting);
    return errCode;
}
