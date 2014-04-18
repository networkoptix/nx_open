#include "media_dewarping_params.h"

#include <utils/common/container.h>
#include <utils/serialization/model_functions.h>
#include <utils/serialization/json.h>
#include <utils/fusion/fusion_adaptor.h>

QN_DEFINE_ENUM_CAST_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(QnMediaDewarpingParams::ViewMode, static)
QN_DEFINE_STRUCT_FUNCTIONS(QnMediaDewarpingParams,      (json)(eq)(debug),    (enabled)(viewMode)(fovRot)(xCenter)(yCenter)(radius))

namespace {

    static const int panoLow      = 1;
    static const int panoNormal   = 2;
    static const int panoWide     = 4;

    static const QList<int> horizontalPano(QList<int>()
                                           << panoLow
                                           << panoNormal
                                           );

    static const QList<int> verticalPano(QList<int>()
                                         << panoLow
                                         << panoNormal
                                         << panoWide
                                         );
}

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

const QList<int>& QnMediaDewarpingParams::allowedPanoFactorValues() const {
    return allowedPanoFactorValues(viewMode);
}

const QList<int>& QnMediaDewarpingParams::allowedPanoFactorValues(ViewMode mode) {
    return (mode == QnMediaDewarpingParams::Horizontal)
            ? horizontalPano
            : verticalPano;
}
