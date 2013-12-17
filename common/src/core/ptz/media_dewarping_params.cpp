#include "media_dewarping_params.h"

#include <utils/common/container.h>
#include <utils/common/model_functions.h>
#include <utils/common/json.h>

QN_DEFINE_ENUM_CAST_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(QnMediaDewarpingParams::ViewMode, static)
QN_DEFINE_STRUCT_FUNCTIONS(QnMediaDewarpingParams,      (json)(eq),    (enabled)(viewMode)(fovRot))

QnMediaDewarpingParams QnMediaDewarpingParams::deserialized(const QByteArray &data) {
    if(data.startsWith('0') || data.startsWith('1')) {
           /* Old format. */
        QnMediaDewarpingParams result;
        QList<QByteArray> params = data.split(';');
        qnResizeList(params, 6);
        result.enabled          = params[0].toInt() > 0;
        result.viewMode         = (ViewMode) params[1].toInt();
        result.fovRot           = params[5].toDouble();
        return result;
    }
    return QJson::deserialized<QnMediaDewarpingParams>(data);
}

