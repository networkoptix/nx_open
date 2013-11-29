#include "ptz_mapper.h"

#include <utils/common/json.h>
#include <utils/common/enum_name_mapper.h>

QN_DEFINE_METAOBJECT_ENUM_NAME_MAPPING(Qn, ExtrapolationMode)
QN_DEFINE_ENUM_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::ExtrapolationMode)

QN_DEFINE_NAME_MAPPED_ENUM(AngleSpace,
    ((DegreesSpace,     "Degrees"))
    ((Mm35EquivSpace,   "35MmEquiv"))
)
QN_DEFINE_ENUM_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(AngleSpace)

/**
 * \param mm35Equiv                 Width-based 35mm-equivalent focal length.
 * \returns                         Width-based FOV in degrees.
 */
static qreal mm35EquivToFov(qreal mm35Equiv) {
    return std::atan((36.0 / 2.0) / mm35Equiv) * 2.0;
}

typedef boost::array<QnSpaceMapperPtr<qreal>, 3> PtzMapperPart;


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

    Qn::ExtrapolationMode extrapolationMode;
    QList<qreal> device, logical;
    qreal deviceMultiplier = 1.0, logicalMultiplier = 1.0; 
    AngleSpace space = DegreesSpace;
    if(
        !QJson::deserialize(map, "extrapolationMode", &extrapolationMode) || 
        !QJson::deserialize(map, "device", &device) ||
        !QJson::deserialize(map, "logical", &logical) ||
        !QJson::deserialize(map, "deviceMultiplier", &deviceMultiplier, true) ||
        !QJson::deserialize(map, "logicalMultiplier", &logicalMultiplier, true) ||
        !QJson::deserialize(map, "space", &space, true)
    ) {
        return false;
    }
    if(device.size() != logical.size())
        return false;

    for(int i = 0; i < logical.size(); i++) {
        logical[i] = logicalMultiplier * logical[i];
        device[i] = deviceMultiplier * device[i];
    }

    if(space == Mm35EquivSpace)
        for(int i = 0; i < logical.size(); i++)
            logical[i] = mm35EquivToFov(logical[i]);

    QVector<QPair<qreal, qreal> > sourceToTarget;
    for(int i = 0; i < logical.size(); i++)
        sourceToTarget.push_back(qMakePair(device[i], logical[i]));

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
