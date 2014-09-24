#include "media_dewarping_params.h"

#include <utils/common/collection.h>
#include <utils/common/model_functions.h>

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(QnMediaDewarpingParams, ViewMode)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnMediaDewarpingParams,      (json)(eq)(debug),    (enabled)(viewMode)(fovRot)(xCenter)(yCenter)(radius)(hStretch))



namespace {
    static const int panoLow      = 1;
    static const int panoNormal   = 2;
    static const int panoWide     = 4;

    static const QList<int> horizontalPano(QList<int>() << panoLow << panoNormal);
    static const QList<int> verticalPano(QList<int>() << panoLow << panoNormal << panoWide);
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
