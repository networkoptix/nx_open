#include "onvif_config_data.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((QnOnvifConfigData), (json), _Fields)

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnOnvifConfigDataPtr *target)
{
    if(value.type() == QJsonValue::Null)
    {
        *target = QnOnvifConfigDataPtr();
        return true;
    }

    QnOnvifConfigData data;
    if(!QJson::deserialize(ctx, value, &data))
        return false;

    *target = QnOnvifConfigDataPtr(new QnOnvifConfigData(std::move(data)));
    return true;
}

void serialize(QnJsonContext *, const QnOnvifConfigDataPtr &, QJsonValue *)
{
    NX_ASSERT(false); /* Not supported for now. */
}
