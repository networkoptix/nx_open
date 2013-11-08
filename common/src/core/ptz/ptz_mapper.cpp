#include "ptz_mapper.h"

#include <utils/common/json.h>
#include <utils/common/enum_name_mapper.h>

namespace {
    Q_GLOBAL_STATIC_WITH_ARGS(QnEnumNameMapper, qn_extrapolationMode_enumNameMapper, (&Qn::staticMetaObject, "ExtrapolationMode"));

    typedef boost::array<QnSpaceMapperPtr<qreal>, 3> PtzMapperPart;

} // anonymous namespace

QnPtzMapper::QnPtzMapper(const QnSpaceMapperPtr<QVector3D> &logicalToDevice, const QnSpaceMapperPtr<QVector3D> &deviceToLogical):
    m_deviceToLogical(deviceToLogical),
    m_logicalToDevice(logicalToDevice)
{
    QVector3D lo = m_logicalToDevice->targetToSource(m_logicalToDevice->sourceToTarget(QVector3D(-180, -90, 0)));
    QVector3D hi = m_logicalToDevice->targetToSource(m_logicalToDevice->sourceToTarget(QVector3D(180, 90, 360)));

    m_logicalLimits.minPan = lo.x();
    m_logicalLimits.maxPan = hi.x();
    m_logicalLimits.minTilt = lo.y();
    m_logicalLimits.maxTilt = hi.y();
    m_logicalLimits.minFov = lo.z();
    m_logicalLimits.maxFov = hi.z();
}

bool deserialize(const QJsonValue &value, QnSpaceMapperPtr<qreal> *target) {
    if(value.type() == QJsonValue::Null) {
        /* That's null mapper. */
        *target = QnSpaceMapperPtr<qreal>();
        return true;
    }

    QJsonObject map;
    if(!QJson::deserialize(value, &map))
        return false;

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

    *target = QnSpaceMapperPtr<qreal>(new QnScalarInterpolationSpaceMapper<qreal>(sourceToTarget, static_cast<Qn::ExtrapolationMode>(extrapolationMode)));
    return true;
}

bool deserialize(const QJsonValue &value, PtzMapperPart *target) {
    if(value.type() == QJsonValue::Null) {
        *target = PtzMapperPart();
        return true;
    }

    QJsonObject map;
    if(!QJson::deserialize(value, &map))
        return false;

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

bool deserialize(const QJsonValue &value, QnPtzMapperPtr *target) {
    if(value.type() == QJsonValue::Null) {
        /* That's null mapper. */
        *target = QnPtzMapperPtr();
        return true;
    }

    QJsonObject map;
    if(!QJson::deserialize(value, &map))
        return false;

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

    *target = QnPtzMapperPtr(new QnPtzMapper(
        QnSpaceMapperPtr<QVector3D>(new QnSeparableVectorSpaceMapper(fromCamera[0], fromCamera[1], fromCamera[2])),
        QnSpaceMapperPtr<QVector3D>(new QnSeparableVectorSpaceMapper(toCamera[0], toCamera[1], toCamera[2]))
    ));
    return true;
}
