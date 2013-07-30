#include "dewarping_params.h"

#include <utils/common/container.h>

bool operator==(const DewarpingParams &l, const DewarpingParams &r) {
    if (l.enabled != r.enabled)
        return false;
    if (l.horizontalView != r.horizontalView)
        return false;
    if (!qFuzzyCompare(l.xAngle, r.xAngle))
        return false;
    if (!qFuzzyCompare(l.yAngle, r.yAngle))
        return false;
    if (!qFuzzyCompare(l.fovRot, r.fovRot))
        return false;
    if (!qFuzzyCompare(l.fov, r.fov))
        return false;
    if (!qFuzzyCompare(l.panoFactor, r.panoFactor))
        return false;

    return true;
}

QByteArray DewarpingParams::serialize() const {
    /* V0. */
    QByteArray result;
    result.append(enabled ? '1' : '0').append(';');
    result.append(horizontalView ? '1' : '0').append(';');
    result.append(QByteArray::number(xAngle)).append(';');
    result.append(QByteArray::number(xAngle)).append(';');
    result.append(QByteArray::number(fov)).append(';');
    result.append(QByteArray::number(fovRot)).append(';');
    result.append(QByteArray::number(panoFactor)).append(';');
    return result;
}

DewarpingParams DewarpingParams::deserialize(const QByteArray &data) {
    DewarpingParams result;

    if(data.startsWith('0') || data.startsWith('1')) {
        /* V0. */
        QList<QByteArray> params = data.split(';');
        resizeList(params, 7);
        result.enabled          = params[0].toInt() > 0;
        result.horizontalView   = params[1].toInt() > 0;
        result.xAngle           = params[2].toDouble();
        result.yAngle           = params[3].toDouble();
        result.fov              = params[4].toDouble();
        result.fovRot           = params[5].toDouble();
        result.panoFactor       = params[6].isEmpty() ? 1.0 : params[6].toDouble();
    }

    return result;
}
