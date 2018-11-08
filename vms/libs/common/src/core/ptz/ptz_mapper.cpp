#include "ptz_mapper.h"

#include <cassert>
#include <array>

#include <utils/math/math.h>
#include <nx/fusion/model_functions.h>

#include <nx/core/ptz/space_mapper.h>

#include "ptz_math.h"

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qn, ExtrapolationMode)

QN_DEFINE_LEXICAL_ENUM(AngleSpace,
    (DegreesSpace,     "Degrees")
    (Mm35EquivSpace,   "35MmEquiv")
)

typedef std::array<QnSpaceMapperPtr<qreal>, 4> PtzMapperPart;


QnPtzMapper::QnPtzMapper(
    const QnSpaceMapperPtr<nx::core::ptz::Vector>& inputMapper,
    const QnSpaceMapperPtr<nx::core::ptz::Vector>& outputMapper)
    :
    m_inputMapper(inputMapper),
    m_outputMapper(outputMapper)
{
    /* OK, I know that this check sucks, but I really didn't want to
     * extend the space mapper interface to make it simpler. */
    qreal minPan = 36000.0, maxPan = -36000.0;
    for(int pan = -360; pan <= 360; pan++)
    {
        const auto pos = m_inputMapper->sourceToTarget(
            m_inputMapper->targetToSource(nx::core::ptz::Vector(pan, 0, 0, 0)));

        minPan = qMin(pos.pan, minPan);
        maxPan = qMax(pos.pan, maxPan);
    }

    if(qFuzzyCompare(maxPan - minPan, 720.0))
    {
        /* There are no limits for pan. */
        m_logicalLimits.minPan = 0.0;
        m_logicalLimits.maxPan = 360.0;
    }
    else
    {
        m_logicalLimits.minPan = minPan;
        m_logicalLimits.maxPan = maxPan;
    }

    auto lo = m_inputMapper->sourceToTarget(
        m_inputMapper->targetToSource(nx::core::ptz::Vector(0,-90, 0, 0)));

    auto hi = m_inputMapper->sourceToTarget(m_inputMapper->targetToSource(
        nx::core::ptz::Vector(0, 90, 0, 360)));

    m_logicalLimits.minTilt = lo.tilt;
    m_logicalLimits.maxTilt = hi.tilt;
    m_logicalLimits.minFov = lo.zoom;
    m_logicalLimits.maxFov = hi.zoom;
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
        !QJson::deserialize(ctx, map, lit("x"), &local[0], /*optional*/ true) ||
        !QJson::deserialize(ctx, map, lit("y"), &local[1], /*optional*/ true) ||
        !QJson::deserialize(ctx, map, lit("r"), &local[2], /*optional*/ true) ||
        !QJson::deserialize(ctx, map, lit("z"), &local[3], /*optional*/ true)
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

    // Null means "try to take this one from the other mapper
    // and use identity mapper if the other mapper is unavailable".
    for(int i = 0; i < input.size(); ++i)
    {
        if(!input[i])
        {
            input[i] = output[i]
                ? output[i]
                : QnSpaceMapperPtr<qreal>(new QnIdentitySpaceMapper<qreal>());
        }

        if(!output[i])
            output[i] = input[i];
    }

    QnSpaceMapperPtr<nx::core::ptz::Vector> inputMapper(
        new nx::core::ptz::SpaceMapper(input[0], input[1], input[2], input[3]));

    QnSpaceMapperPtr<nx::core::ptz::Vector> outputMapper(
        new nx::core::ptz::SpaceMapper(output[0], output[1], output[2], output[3]));

    *target = QnPtzMapperPtr(new QnPtzMapper(inputMapper, outputMapper));
    return true;
}

void serialize(QnJsonContext *, const QnPtzMapperPtr &, QJsonValue *) {
    NX_ASSERT(false); /* Not supported for now. */
}
