#ifndef QN_FUSION_REST_HANDLER_H
#define QN_FUSION_REST_HANDLER_H

#include "request_handler.h"

#include <utils/serialization/lexical_functions.h>
#include <nx/network/http/httptypes.h>
#include <utils/common/model_functions.h>
#include "utils/common/util.h"
#include "json_rest_result.h"

namespace QnFusionRestHandlerDetail
{
    Qn::SerializationFormat formatFromParams(const QnRequestParamList& params);

    template <class OutputData>
    void serialize(const OutputData& outputData, QByteArray& result, QByteArray& contentType, Qn::SerializationFormat format = Qn::UnsupportedFormat, bool extraFormatting = false)
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
            NX_ASSERT(0);
        }
        contentType = Qn::serializationFormatToHttpContentType(format);
    }

    template <class OutputData>
    void serializeRestReply(const OutputData& outputData, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestResult& _restResult)
    {
        Qn::SerializationFormat format = formatFromParams(params);
        switch(format)
        {
        case Qn::UbjsonFormat:
            {
                QnUbjsonRestResult restReply(_restResult);
                restReply.setReply(outputData);
                result = QnUbjson::serialized(restReply);
                break;
            }
        case Qn::JsonFormat:
            {
                QnJsonRestResult restReply(_restResult);
                restReply.setReply(outputData);
                result = QJson::serialized(restReply);
                if (params.contains(lit("extraFormatting")))
                    formatJSonString(result);
                break;
            }
        default:
            break; // not implemented, no content data
        }
        contentType = Qn::serializationFormatToHttpContentType(format);
    }

}

class QnFusionRestHandler: public QnRestRequestHandler
{
public:
    QnFusionRestHandler() {}
    virtual ~QnFusionRestHandler() {}

    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result,
                            QByteArray& resultContentType, const QnRestConnectionProcessor *processor)  override;

    virtual int executeGet(const QString& path, const QnRequestParamList& params,
        QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;

protected:
    static int genericError(int errCode, const QString &error, QByteArray& result, QByteArray& contentType, Qn::SerializationFormat format = Qn::UnsupportedFormat, bool extraFormatting = false );
};


#endif // QN_FUSION_REST_HANDLER_H
