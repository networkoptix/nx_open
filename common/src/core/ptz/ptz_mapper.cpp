#include "ptz_mapper.h"

#include <utils/common/json.h>
#include <utils/common/enum_name_mapper.h>
#include <utils/math/math.h>

QN_DEFINE_METAOBJECT_ENUM_NAME_MAPPING(Qn, ExtrapolationMode)
QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(Qn::ExtrapolationMode)

QN_DEFINE_NAME_MAPPED_ENUM(AngleSpace,
    ((DegreesSpace,     "Degrees"))
    ((Mm35EquivSpace,   "35MmEquiv"))
)
QN_DEFINE_ENUM_MAPPED_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(AngleSpace)


/**
 * \param mm35Equiv                 Width-based 35mm-equivalent focal length.
 * \returns                         Width-based FOV in degrees.
 */
static qreal mm35EquivToDegrees(qreal mm35Equiv) {
    return qRadiansToDegrees(std::atan((36.0 / 2.0) / mm35Equiv) * 2.0);
}

typedef boost::array<QnSpaceMapperPtr<qreal>, 3> PtzMapperPart;


QnPtzMapper::QnPtzMapper(const QnSpaceMapperPtr<QVector3D> &inputMapper, const QnSpaceMapperPtr<QVector3D> &outputMapper):
    m_inputMapper(inputMapper),
    m_outputMapper(outputMapper)
{
    /* OK, I know that this check sucks, but I really didn't want to
     * extend the space mapper interface to make it simpler. */
    qreal minPan = 36000.0, maxPan = -36000.0;
    for(int pan = 0; pan <= 360; pan++) {
        QVector3D pos = m_inputMapper->sourceToTarget(m_inputMapper->targetToSource(QVector3D(pan, 0, 0)));
        minPan = qMin(pos.x(), minPan);
        maxPan = qMax(pos.x(), maxPan);
    }
    if(qFuzzyCompare(maxPan - minPan, 360.0)) {
        /* There are no limits for pan. */
        m_logicalLimits.minPan = 0.0;
        m_logicalLimits.maxPan = 360.0;
    } else {
        m_logicalLimits.minPan = minPan;
        m_logicalLimits.maxPan = maxPan;
    }

    QVector3D lo = m_inputMapper->sourceToTarget(m_inputMapper->targetToSource(QVector3D(0, -90, 0)));
    QVector3D hi = m_inputMapper->sourceToTarget(m_inputMapper->targetToSource(QVector3D(0, 90, 360)));
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
        !QJson::deserialize(map, lit("extrapolationMode"), &extrapolationMode) || 
        !QJson::deserialize(map, lit("device"), &device) ||
        !QJson::deserialize(map, lit("logical"), &logical) ||
        !QJson::deserialize(map, lit("deviceMultiplier"), &deviceMultiplier, true) ||
        !QJson::deserialize(map, lit("logicalMultiplier"), &logicalMultiplier, true) ||
        !QJson::deserialize(map, lit("space"), &space, true)
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
            logical[i] = mm35EquivToDegrees(logical[i]);

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
        !QJson::deserialize(map, lit("x"), &local[0], true) || 
        !QJson::deserialize(map, lit("y"), &local[1], true) ||
        !QJson::deserialize(map, lit("z"), &local[2], true)
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

    PtzMapperPart input, output;
    if(
        !QJson::deserialize(map, lit("fromCamera"), &output, true) ||
        !QJson::deserialize(map, lit("toCamera"), &input, true)
    ) {
        return false;
    }

    /* Null means "take this one from the other mapper". */
    for(int i = 0; i < 3; i++) {
        if(!input[i]) {
            if(!output[i])
                return false;

            input[i] = output[i];
        } else if(!output[i]) {
            output[i] = input[i];
        }
    }

    *target = QnPtzMapperPtr(new QnPtzMapper(
        QnSpaceMapperPtr<QVector3D>(new QnSeparableVectorSpaceMapper(input[0], input[1], input[2])),
        QnSpaceMapperPtr<QVector3D>(new QnSeparableVectorSpaceMapper(output[0], output[1], output[2]))
    ));
    return true;
}
