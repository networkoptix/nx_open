#ifndef QN_FUSION_REST_HANDLER_H
#define QN_FUSION_REST_HANDLER_H

#include "request_handler.h"

#include <utils/serialization/lexical_functions.h>
#include "utils/network/http/httptypes.h"
#include <utils/common/model_functions.h>
#include "utils/common/util.h"

namespace QnFusionRestHandlerDetail
{
    Qn::SerializationFormat formatFromParams(const QnRequestParamList& params);

    template <class OutputData>
    void serialize(const OutputData& outputData, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
    {
        Qn::SerializationFormat format = formatFromParams(params);
        switch(format) 
        {
        case Qn::UbjsonFormat:
            result = QnUbjson::serialized(outputData);
            break;
        case Qn::JsonFormat:
            result = QJson::serialized(outputData);
            if (params.contains(lit("extraFormatting")))
                formatJSonString(result);
            break;
        case Qn::CsvFormat:
            result = QnCsv::serialized(outputData);
            break;
        case Qn::XmlFormat:
            result = QnXml::serialized(outputData, lit("reply"));
            break;
        default:
            assert(0);
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
};


#endif // QN_FUSION_REST_HANDLER_H
