#include "space_mapper.h"

#include <cassert>

#include <utils/common/enum_name_mapper.h>
#include <utils/common/json.h>

namespace {
    Q_GLOBAL_STATIC_WITH_ARGS(QnEnumNameMapper, qn_extrapolationMode_enumNameMapper, (qnEnumNameMapper(QnCommonGlobals, ExtrapolationMode)));
}

// -------------------------------------------------------------------------- //
// QnScalarSpaceMapper
// -------------------------------------------------------------------------- //
QnScalarSpaceMapper::QnScalarSpaceMapper(qreal logical0, qreal logical1, qreal physical0, qreal physical1, Qn::ExtrapolationMode extrapolationMode) {
    QVector<QPointF> logicalToPhysical;
    logicalToPhysical.push_back(QPointF(logical0, physical0));
    logicalToPhysical.push_back(QPointF(logical1, physical1));
    init(logicalToPhysical, extrapolationMode);
}

void QnScalarSpaceMapper::init(const QVector<QPointF> &logicalToPhysical, Qn::ExtrapolationMode extrapolationMode) {
    foreach(const QPointF &point, logicalToPhysical) {
        m_logicalToPhysical.addPoint(point.x(), point.y());
        m_physicalToLogical.addPoint(point.y(), point.x());
    }
    m_logicalToPhysical.setExtrapolationMode(extrapolationMode);
    m_physicalToLogical.setExtrapolationMode(extrapolationMode);

    qreal logicalA, logicalB, physicalA, physicalB;
    if(m_logicalToPhysical.points().empty()) {
        logicalA = logicalB = m_physicalToLogical(0.0);
        physicalA = physicalB = m_logicalToPhysical(0.0);
    } else {
        int last = logicalToPhysical.size() - 1;

        logicalA = m_logicalToPhysical.point(0).x();
        logicalB = m_logicalToPhysical.point(last).x();
        physicalA = m_physicalToLogical.point(0).x();
        physicalB = m_physicalToLogical.point(last).x();
    }

    m_logicalMinimum = qMin(logicalA, logicalB);
    m_logicalMaximum = qMax(logicalA, logicalB);
    m_physicalMinimum = qMin(physicalA, physicalB);
    m_physicalMaximum = qMax(physicalA, physicalB);
}

void serialize(const QnScalarSpaceMapper &value, QVariant *target) {
    assert(target);

    QString extrapolationMode = qn_extrapolationMode_enumNameMapper()->name(value.logicalToPhysical().extrapolationMode());

    QVariantList logical, physical;
    foreach(const QPointF &point, value.logicalToPhysical().points()) {
        logical.push_back(point.x());
        physical.push_back(point.y());
    }

    QVariantMap result;
    result[lit("extrapolationMode")] = extrapolationMode;
    result[lit("logical")] = logical;
    result[lit("physical")] = physical;
    *target = result;
}

bool deserialize(const QVariant &value, QnScalarSpaceMapper *target) {
    assert(target);

    QVariantMap map = value.toMap();
    QString extrapolationModeName = map.value(lit("extrapolationMode")).toString();
    QVariantList logical = map.value(lit("logical")).toList();
    QVariantList physical = map.value(lit("physical")).toList();
    
    if(map.isEmpty() || extrapolationModeName.isEmpty() || logical.size() != physical.size())
        return false;

    int extrapolationMode = qn_extrapolationMode_enumNameMapper()->value(extrapolationModeName);
    if(extrapolationMode == -1)
        return false;

    QVector<QPointF> logicalToPhysical;
    for(int i = 0; i < logical.size(); i++) {
        bool success = true;
        qreal x = logical[i].toReal(&success);
        qreal y = physical[i].toReal(&success);
        if(!success)
            return false;
        logicalToPhysical.push_back(QPointF(x, y));
    }

    *target = QnScalarSpaceMapper(logicalToPhysical, static_cast<Qn::ExtrapolationMode>(extrapolationMode));
    return true;
}


// -------------------------------------------------------------------------- //
// QnVectorSpaceMapper
// -------------------------------------------------------------------------- //
void serialize(const QnVectorSpaceMapper &value, QVariant *target) {
    assert(target);

    QVariantMap result;
    serialize(value.mapper(QnVectorSpaceMapper::X), &result[lit("x")]);
    serialize(value.mapper(QnVectorSpaceMapper::Y), &result[lit("y")]);
    serialize(value.mapper(QnVectorSpaceMapper::Z), &result[lit("z")]);
    *target = result;
}

bool deserialize(const QVariant &value, QnVectorSpaceMapper *target) {
    assert(target);

    QVariantMap map = value.toMap();
    
    QnScalarSpaceMapper xMapper, yMapper, zMapper;
    if(
        !deserialize(map.value(lit("x")), &xMapper) || 
        !deserialize(map.value(lit("y")), &yMapper) ||
        !deserialize(map.value(lit("z")), &zMapper)
    ) {
        return false;
    }

    *target = QnVectorSpaceMapper(xMapper, yMapper, zMapper);
    return true;
}


// -------------------------------------------------------------------------- //
// QnPtzSpaceMapper
// -------------------------------------------------------------------------- //
void serialize(const QnPtzSpaceMapper &value, QVariant *target) {
    assert(target);

    QVariantMap result;
    serialize(value.models(), &result[lit("models")]);
    serialize(value.fromCamera(), &result[lit("fromCamera")]);
    serialize(value.toCamera(), &result[lit("toCamera")]);
    *target = result;
}

bool deserialize(const QVariant &value, QnPtzSpaceMapper *target) {
    assert(target);

    QVariantMap map = value.toMap();

    QStringList models;
    QnVectorSpaceMapper toCamera, fromCamera;
    if(
        !deserialize(map.value(lit("models")), &models) || 
        !deserialize(map.value(lit("fromCamera")), &fromCamera) ||
        !deserialize(map.value(lit("toCamera")), &toCamera)
    ) {
        return false;
    }

    *target = QnPtzSpaceMapper(fromCamera, toCamera, models);
    return true;
}
