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
