#pragma once

#include <nx/fusion/serialization/lexical_functions.h>
#include <nx/network/http/http_types.h>
#include <nx/fusion/model_functions.h>
#include <utils/common/util.h>

#include "request_handler.h"
#include "json_rest_result.h"

namespace QnFusionRestHandlerDetail {

Qn::SerializationFormat formatFromParams(const QnRequestParamList& params);

template <class OutputData>
static void serialize(
    const OutputData& outputData, QByteArray& result, QByteArray& contentType,
    Qn::SerializationFormat format = Qn::UnsupportedFormat, bool extraFormatting = false)
{
    if (format == Qn::UnsupportedFormat)
        format = Qn::JsonFormat;

    switch(format)
    {
        case Qn::UbjsonFormat:
            result = QnUbjson::serialized(outputData);
            break;
        case Qn::JsonFormat:
            result = QJson::serialized(outputData);
            if (extraFormatting)
                formatJSonString(result);
            break;
        case Qn::CsvFormat:
            result = QnCsv::serialized(outputData);
            break;
        case Qn::XmlFormat:
            result = QnXml::serialized(outputData, lit("reply"));
            break;
        default:
            NX_ASSERT(false);
    }
    contentType = Qn::serializationFormatToHttpContentType(format);
}

template <class OutputData>
static void serializeJsonRestReply(
    const OutputData& outputData, const QnRequestParamList& params, QByteArray& result,
    QByteArray& contentType, const QnRestResult& restResult)
{
    QnJsonRestResult jsonRestResult(restResult);
    jsonRestResult.setReply(outputData);
    result = QJson::serialized(jsonRestResult);
    if (params.contains(lit("extraFormatting")))
        formatJSonString(result);

    contentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
}

template <class OutputData>
static void serializeRestReply(
    const OutputData& outputData, const QnRequestParamList& params, QByteArray& result,
    QByteArray& contentType, const QnRestResult& restResult)
{
    Qn::SerializationFormat format = formatFromParams(params);
    switch(format)
    {
        case Qn::UbjsonFormat:
        {
            QnUbjsonRestResult ubjsonRestResult(restResult);
            ubjsonRestResult.setReply(outputData);
            result = QnUbjson::serialized(ubjsonRestResult);
            break;
        }

        case Qn::JsonFormat:
            serializeJsonRestReply(outputData, params, result, contentType, restResult);
            return;

        default:
            break; //< not implemented, no content data
    }
    contentType = Qn::serializationFormatToHttpContentType(format);
}

} // namespace QnFusionRestHandlerDetail

class QnFusionRestHandler: public QnRestRequestHandler
{
public:
    QnFusionRestHandler() {}
    virtual ~QnFusionRestHandler() {}

    virtual int executePost(
        const QString& path, const QnRequestParamList& params, const QByteArray& body,
        const QByteArray& srcBodyContentType, QByteArray& result,
        QByteArray& resultContentType, const QnRestConnectionProcessor* processor)  override;

    virtual int executeGet(
        const QString& path, const QnRequestParamList& params, QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor* processor) override;

    static int makeError(
        int httpStatusCode, const QString &errorMessage, QByteArray* outBody,
        QByteArray* outContentType, Qn::SerializationFormat format, bool extraFormatting = false,
        QnRestResult::Error error = QnRestResult::CantProcessRequest);
};
