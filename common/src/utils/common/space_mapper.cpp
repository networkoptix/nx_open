#include "space_mapper.h"

#include <utils/common/enum_name_mapper.h>

namespace {
    Q_GLOBAL_STATIC_WITH_ARGS(QnEnumNameMapper, qn_extrapolationMode_enumNameMapper, (qnEnumNameMapper(QnCommonGlobals, ExtrapolationMode)));
}

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

QVariant QnScalarSpaceMapper::serialize(const QnScalarSpaceMapper &value) {
    QString extrapolationMode = qn_extrapolationMode_enumNameMapper()->name(value.m_logicalToPhysical.extrapolationMode());

    QVariantList logical, physical;
    foreach(const QPointF &point, value.m_logicalToPhysical.points()) {
        logical.push_back(point.x());
        physical.push_back(point.y());
    }

    QVariantMap result;
    result[lit("extrapolationMode")] = extrapolationMode;
    result[lit("logical")] = logical;
    result[lit("physical")] = physical;

    return result;
}

QnScalarSpaceMapper QnScalarSpaceMapper::deserialize(const QVariant &value, bool *ok) {
    QVariantMap map = value.toMap();
    QString extrapolationModeName = map.value(lit("extrapolationMode")).toString();
    QVariantList logical = map.value(lit("logical")).toList();
    QVariantList physical = map.value(lit("physical")).toList();
    
    QVector<QPointF> logicalToPhysical;
    int extrapolationMode;

    if(map.isEmpty() || extrapolationModeName.isEmpty() || logical.size() != physical.size())
        goto error;

    extrapolationMode = qn_extrapolationMode_enumNameMapper()->value(extrapolationModeName);
    if(extrapolationMode == -1)
        goto error;

    for(int i = 0; i < logical.size(); i++) {
        bool success = true;
        qreal x = logical[i].toReal(&success);
        qreal y = physical[i].toReal(&success);
        if(!success)
            goto error;
        logicalToPhysical.push_back(QPointF(x, y));
    }

    if(ok)
        *ok = true;
    return QnScalarSpaceMapper(logicalToPhysical, static_cast<Qn::ExtrapolationMode>(extrapolationMode));

error:
    if(ok)
        *ok = false;
    return QnScalarSpaceMapper();
}


