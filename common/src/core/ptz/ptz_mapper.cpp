#include "ptz_mapper.h"

#include <utils/common/enum_name_mapper.h>

namespace {
    Q_GLOBAL_STATIC_WITH_ARGS(QnEnumNameMapper, qn_extrapolationMode_enumNameMapper, (&Qn::staticMetaObject, "ExtrapolationMode"));

    typedef boost::array<QnSpaceMapperPtr<qreal>, 3> PtzMapperPart;

} // anonymous namespace

bool deserialize(const QVariant &value, QnSpaceMapperPtr<qreal> *logical) {
    if(value.type() == QVariant::Invalid) {
        /* That's null mapper. */
        *logical = QnSpaceMapperPtr<qreal>();
        return true;
    }

    if(value.type() != QVariant::Map)
        return false;
    QVariantMap map = value.toMap();

    /* Note: source == device space, target == logical space. */

    QString extrapolationModeName;
    QList<qreal> device, logical;
    qreal deviceMultiplier = 1.0, logicalMultiplier = 1.0; 
    if(
        !QJson::deserialize(map, "extrapolationMode", &extrapolationModeName) || 
        !QJson::deserialize(map, "device", &device) ||
        !QJson::deserialize(map, "logical", &logical) ||
        !QJson::deserialize(map, "deviceMultiplier", &deviceMultiplier, true) ||
        !QJson::deserialize(map, "logicalMultiplier", &logicalMultiplier, true)
    ) {
        return false;
    }
    if(device.size() != logical.size())
        return false;

    int extrapolationMode = qn_extrapolationMode_enumNameMapper()->value(extrapolationModeName, -1);
    if(extrapolationMode == -1)
        return false;

    QVector<QPair<qreal, qreal> > sourceToTarget;
    for(int i = 0; i < device.size(); i++)
        sourceToTarget.push_back(qMakePair(device[i] * deviceMultiplier, logical[i] * logicalMultiplier));

    *logical = QnSpaceMapperPtr<qreal>(new QnScalarInterpolationSpaceMapper(sourceToTarget, static_cast<Qn::ExtrapolationMode>(extrapolationMode)));
    return true;
}

bool deserialize(const QVariant &value, PtzMapperPart *target) {
    if(value.type() == QVariant::Invalid) {
        *target = PtzMapperPart();
        return true;
    }

    if(value.type() != QVariant::Map)
        return false;
    QVariantMap map = value.toMap();

    PtzMapperPart local;
    if(
        !QJson::deserialize(map, "x", &local[0], true) || 
        !QJson::deserialize(map, "y", &local[1], true) ||
        !QJson::deserialize(map, "z", &local[2], true)
    ) {
            return false;
    }

    *target = local;
    return true;
}

bool deserialize(const QVariant &value, QnPtzMapperPtr *target) {
    if(value.type() == QVariant::Invalid) {
        /* That's null mapper. */
        *target = QnPtzMapperPtr();
        return true;
    }

    if(value.type() != QVariant::Map)
        return false;
    QVariantMap map = value.toMap();

    PtzMapperPart toCamera, fromCamera;
    if(
        !QJson::deserialize(map, "fromCamera", &fromCamera, true) ||
        !QJson::deserialize(map, "toCamera", &toCamera, true)
    ) {
        return false;
    }

    /* Null means "take this one from the other mapper". */
    for(int i = 0; i < 3; i++) {
        if(!toCamera[0]) {
            if(!fromCamera[0])
                return false;

            toCamera[0] = fromCamera[0];
        } else if(!fromCamera[0]) {
            fromCamera[0] = toCamera[0];
        }
    }

    *target = QnPtzMapperPtr(new QnAssymetricSpaceMapper<QVector3D>(
        QnPtzMapperPtr(new QnSeparableVectorSpaceMapper(fromCamera[0], fromCamera[1], fromCamera[2])),
        QnPtzMapperPtr(new QnSeparableVectorSpaceMapper(toCamera[0], toCamera[1], toCamera[2])),
    ));
    return true;
}
