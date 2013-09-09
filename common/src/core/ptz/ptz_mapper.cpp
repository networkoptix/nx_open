#include "ptz_mapper.h"

namespace {
    Q_GLOBAL_STATIC_WITH_ARGS(QnEnumNameMapper, qn_extrapolationMode_enumNameMapper, (&Qn::staticMetaObject, "ExtrapolationMode"));
}

bool deserialize(const QVariant &value, QnSpaceMapperPtr<qreal> *target) {
    if(value.type() == QVariant::Invalid) {
        /* That's null mapper. */
        *target = QnSpaceMapperPtr<qreal>();
        return true;
    }

    if(value.type() != QVariant::Map)
        return false;
    QVariantMap map = value.toMap();

    /* Note: source == device space, target == logical space. */

    QString extrapolationModeName;
    QList<qreal> source, target;
    qreal sourceMultiplier = 1.0, targetMultiplier = 1.0; 
    if(
        !QJson::deserialize(map, "extrapolationMode", &extrapolationModeName) || 
        !QJson::deserialize(map, "source", &source) ||
        !QJson::deserialize(map, "target", &target) ||
        !QJson::deserialize(map, "sourceMultiplier", &sourceMultiplier, true) ||
        !QJson::deserialize(map, "targetMultiplier", &targetMultiplier, true)
    ) {
        return false;
    }
    if(source.size() != target.size())
        return false;

    int extrapolationMode = qn_extrapolationMode_enumNameMapper()->value(extrapolationModeName, -1);
    if(extrapolationMode == -1)
        return false;

    QVector<QPair<qreal, qreal> > sourceToTarget;
    for(int i = 0; i < source.size(); i++)
        sourceToTarget.push_back(qMakePair(source[i] * sourceMultiplier, target[i] * targetMultiplier));

    *target = QnSpaceMapperPtr<qreal>(new QnScalarInterpolationSpaceMapper(sourceToTarget, static_cast<Qn::ExtrapolationMode>(extrapolationMode)));
    return true;
}

bool deserialize(const QVariant &value, QnPtzMapperPtr *target) {
    
}
