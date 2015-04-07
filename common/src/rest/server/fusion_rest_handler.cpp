#include "fusion_rest_handler.h"

namespace QnFusionRestHandlerDetail
{
    Qn::SerializationFormat formatFromParams(const QnRequestParamList& params)
    {
        Qn::SerializationFormat format = Qn::JsonFormat;
        QnLexical::deserialize(params.value(lit("format")), &format);
        return format;
    }
}

int QnFusionRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, QByteArray& resultContentType, const QnRestConnectionProcessor *processor)
{
    Q_UNUSED(path);
    Q_UNUSED(params);
    Q_UNUSED(body);
    Q_UNUSED(srcBodyContentType);
    Q_UNUSED(result);
    Q_UNUSED(resultContentType);
    Q_UNUSED(processor);
    return nx_http::StatusCode::notImplemented;
}
