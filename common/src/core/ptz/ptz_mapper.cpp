#include "ptz_mapper.h"

#include <cassert>

#include <utils/math/math.h>
#include <utils/common/model_functions.h>

#include "ptz_math.h"

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, ExtrapolationMode)

QN_DEFINE_LEXICAL_ENUM(AngleSpace,
    (DegreesSpace,     "Degrees")
    (Mm35EquivSpace,   "35MmEquiv")
)

typedef boost::array<QnSpaceMapperPtr<qreal>, 3> PtzMapperPart;


QnPtzMapper::QnPtzMapper(const QnSpaceMapperPtr<QVector3D> &inputMapper, const QnSpaceMapperPtr<QVector3D> &outputMapper):
    m_inputMapper(inputMapper),
    m_outputMapper(outputMapper)
{
    /* OK, I know that this check sucks, but I really didn't want to
     * extend the space mapper interface to make it simpler. */
    qreal minPan = 36000.0, maxPan = -36000.0;
    for(int pan = -360; pan <= 360; pan++) {
        QVector3D pos = m_inputMapper->sourceToTarget(m_inputMapper->targetToSource(QVector3D(pan, 0, 0)));
        minPan = qMin(pos.x(), minPan);
        maxPan = qMax(pos.x(), maxPan);
    }
    if(qFuzzyCompare(maxPan - minPan, 720.0)) {
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

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnSpaceMapperPtr<qreal> *target) {
    if(value.type() == QJsonValue::Null) {
        /* That's null mapper. */
        *target = QnSpaceMapperPtr<qreal>();
        return true;
    }

    QJsonObject map;
    if(!QJson::deserialize(ctx, value, &map))
        return false;

    /* Note: source == device space, target == logical space. */

    Qn::ExtrapolationMode extrapolationMode;
    QVector<qreal> device, logical;
    qreal deviceMultiplier = 1.0, logicalMultiplier = 1.0; 
    AngleSpace space = DegreesSpace;
    if(
        !QJson::deserialize(ctx, map, lit("extrapolationMode"), &extrapolationMode) || 
        !QJson::deserialize(ctx, map, lit("device"), &device) ||
        !QJson::deserialize(ctx, map, lit("logical"), &logical) ||
        !QJson::deserialize(ctx, map, lit("deviceMultiplier"), &deviceMultiplier, true) ||
        !QJson::deserialize(ctx, map, lit("logicalMultiplier"), &logicalMultiplier, true) ||
        !QJson::deserialize(ctx, map, lit("space"), &space, true)
    ) {
        return false;
    }
    if(device.size() != logical.size())
        return false;

    for(int i = 0; i < logical.size(); i++) {
        logical[i] = logicalMultiplier * logical[i];
        device[i] = deviceMultiplier * device[i];
    }

    if(space == Mm35EquivSpace) {
        /* What is linear in 35mm-equiv space is non-linear in degrees, 
         * so we compensate by inserting additional data points. */
        while(logical.size() < 16) {
            for(int i = logical.size() - 2; i >= 0; i--) {
                logical.insert(i + 1, (logical[i] + logical[i + 1]) / 2.0);
                device.insert(i + 1, (device[i] + device[i + 1]) / 2.0);
            }
        }

        for(int i = 0; i < logical.size(); i++)
            logical[i] = q35mmEquivToDegrees(logical[i]);
    }

    QVector<QPair<qreal, qreal> > sourceToTarget;
    for(int i = 0; i < logical.size(); i++)
        sourceToTarget.push_back(qMakePair(device[i], logical[i]));

    *target = QnSpaceMapperPtr<qreal>(new QnScalarInterpolationSpaceMapper<qreal>(sourceToTarget, static_cast<Qn::ExtrapolationMode>(extrapolationMode)));
    return true;
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, PtzMapperPart *target) {
    if(value.type() == QJsonValue::Null) {
        *target = PtzMapperPart();
        return true;
    }

    QJsonObject map;
    if(!QJson::deserialize(ctx, value, &map))
        return false;

    PtzMapperPart local;
    if(
        !QJson::deserialize(ctx, map, lit("x"), &local[0], true) || 
        !QJson::deserialize(ctx, map, lit("y"), &local[1], true) ||
        !QJson::deserialize(ctx, map, lit("z"), &local[2], true)
    ) {
            return false;
    }

    *target = local;
    return true;
}

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnPtzMapperPtr *target) {
    if(value.type() == QJsonValue::Null) {
        /* That's null mapper. */
        *target = QnPtzMapperPtr();
        return true;
    }

    QJsonObject map;
    if(!QJson::deserialize(ctx, value, &map))
        return false;

    PtzMapperPart input, output;
    if(
        !QJson::deserialize(ctx, map, lit("fromCamera"), &output, true) ||
        !QJson::deserialize(ctx, map, lit("toCamera"), &input, true)
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

void serialize(QnJsonContext *, const QnPtzMapperPtr &, QJsonValue *) {
    assert(false); /* Not supported for now. */
}
