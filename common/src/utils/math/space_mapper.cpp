#include "space_mapper.h"

#include <utils/common/enum_name_mapper.h>
#include <utils/common/json.h>

namespace {
    Q_GLOBAL_STATIC_WITH_ARGS(QnEnumNameMapper, qn_extrapolationMode_enumNameMapper, (&QnCommonGlobals::staticMetaObject, "ExtrapolationMode"));
}

// -------------------------------------------------------------------------- //
// QnScalarSpaceMapper
// -------------------------------------------------------------------------- //
QnScalarSpaceMapper::QnScalarSpaceMapper(qreal logical0, qreal logical1, qreal physical0, qreal physical1, Qn::ExtrapolationMode extrapolationMode) {
    QVector<QPair<qreal, qreal> > logicalToPhysical;
    logicalToPhysical.push_back(qMakePair(logical0, physical0));
    logicalToPhysical.push_back(qMakePair(logical1, physical1));
    init(logicalToPhysical, extrapolationMode);
}

void QnScalarSpaceMapper::init(const QVector<QPair<qreal, qreal> > &logicalToPhysical, Qn::ExtrapolationMode extrapolationMode) {
    QVector<QPair<qreal, qreal> > physicalToLogical;
    typedef QPair<qreal, qreal> PairType; // TODO: #Elric #C++11 replace with auto
    foreach(const PairType &point, logicalToPhysical)
        physicalToLogical.push_back(qMakePair(point.second, point.first));

    m_logicalToPhysical.setPoints(logicalToPhysical);
    m_logicalToPhysical.setExtrapolationMode(extrapolationMode);
    m_physicalToLogical.setPoints(physicalToLogical);
    m_physicalToLogical.setExtrapolationMode(extrapolationMode);

    qreal logicalA, logicalB, physicalA, physicalB;
    if(m_logicalToPhysical.points().empty()) {
        logicalA = logicalB = m_physicalToLogical(0.0);
        physicalA = physicalB = m_logicalToPhysical(0.0);
    } else {
        int last = logicalToPhysical.size() - 1;

        logicalA = m_logicalToPhysical.point(0).first;
        logicalB = m_logicalToPhysical.point(last).first;
        physicalA = m_physicalToLogical.point(0).first;
        physicalB = m_physicalToLogical.point(last).first;
    }

    m_logicalMinimum = qMin(logicalA, logicalB);
    m_logicalMaximum = qMax(logicalA, logicalB);
    m_physicalMinimum = qMin(physicalA, physicalB);
    m_physicalMaximum = qMax(physicalA, physicalB);
}

void serialize(const QnScalarSpaceMapper &value, QVariant *target) {
    QString extrapolationMode = qn_extrapolationMode_enumNameMapper()->name(value.logicalToPhysical().extrapolationMode());

    QVariantList logical, physical;
    typedef QPair<qreal, qreal> PairType; // TODO: #Elric #C++11 replace with auto
    foreach(const PairType &point, value.logicalToPhysical().points()) {
        logical.push_back(point.first);
        physical.push_back(point.second);
    }

    QVariantMap result;
    result[lit("extrapolationMode")] = extrapolationMode;
    result[lit("logical")] = logical;
    result[lit("physical")] = physical;
    *target = result;
}

bool deserialize(const QVariant &value, QnScalarSpaceMapper *target) {
    if(value.type() == QVariant::Invalid) {
        /* That's null mapper. */
        *target = QnScalarSpaceMapper();
        return true;
    }

    if(value.type() != QVariant::Map)
        return false;
    QVariantMap map = value.toMap();

    QString extrapolationModeName;
    QList<qreal> logical, physical;
    qreal logicalMultiplier = 1.0, physicalMultiplier = 1.0;
    if(
        !QJson::deserialize(map, "extrapolationMode", &extrapolationModeName) || 
        !QJson::deserialize(map, "logical", &logical) ||
        !QJson::deserialize(map, "physical", &physical) ||
        !QJson::deserialize(map, "logicalMultiplier", &logicalMultiplier, true) ||
        !QJson::deserialize(map, "physicalMultiplier", &physicalMultiplier, true)
    ) {
        return false;
    }
    if(logical.size() != physical.size())
        return false;

    int extrapolationMode = qn_extrapolationMode_enumNameMapper()->value(extrapolationModeName, -1);
    if(extrapolationMode == -1)
        return false;

    QVector<QPair<qreal, qreal> > logicalToPhysical;
    for(int i = 0; i < logical.size(); i++)
        logicalToPhysical.push_back(qMakePair(logical[i] * logicalMultiplier, physical[i] * physicalMultiplier));

    *target = QnScalarSpaceMapper(logicalToPhysical, static_cast<Qn::ExtrapolationMode>(extrapolationMode));
    return true;
}


// -------------------------------------------------------------------------- //
// QnVectorSpaceMapper
// -------------------------------------------------------------------------- //
void serialize(const QnVectorSpaceMapper &value, QVariant *target) {
    QVariantMap result;
    QJson::serialize(value.mapper(QnVectorSpaceMapper::X), "x", &result);
    QJson::serialize(value.mapper(QnVectorSpaceMapper::Y), "y", &result);
    QJson::serialize(value.mapper(QnVectorSpaceMapper::Z), "z", &result);
    *target = result;
}

bool deserialize(const QVariant &value, QnVectorSpaceMapper *target) {
    if(value.type() == QVariant::Invalid) {
        /* That's null mapper. */
        *target = QnVectorSpaceMapper();
        return true;
    }

    if(value.type() != QVariant::Map)
        return false;
    QVariantMap map = value.toMap();
    
    QnScalarSpaceMapper xMapper, yMapper, zMapper;
    if(
        !QJson::deserialize(map, "x", &xMapper, true) || 
        !QJson::deserialize(map, "y", &yMapper, true) ||
        !QJson::deserialize(map, "z", &zMapper, true)
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
    QVariantMap result;
    QJson::serialize(value.models(), "models", &result);
    QJson::serialize(value.fromCamera(), "fromCamera", &result);
    QJson::serialize(value.toCamera(), "toCamera", &result);
    *target = result;
}

bool deserialize(const QVariant &value, QnPtzSpaceMapper *target) {
    if(value.type() == QVariant::Invalid) {
        /* That's null mapper. */
        *target = QnPtzSpaceMapper();
        return true;
    }

    if(value.type() != QVariant::Map)
        return false;
    QVariantMap map = value.toMap();

    QStringList models;
    QnVectorSpaceMapper toCamera, fromCamera;
    if(
        !QJson::deserialize(map, "models", &models) || 
        !QJson::deserialize(map, "fromCamera", &fromCamera, true) ||
        !QJson::deserialize(map, "toCamera", &toCamera, true)
    ) {
        return false;
    }

    /* Null means "take this one from the other mapper". */
    if(toCamera.isNull() && fromCamera.isNull())
        return false;
    if(fromCamera.isNull())
        fromCamera = toCamera;
    if(toCamera.isNull())
        toCamera = fromCamera;
    for(int i = 0; i < QnVectorSpaceMapper::CoordinateCount; i++) {
        QnVectorSpaceMapper::Coordinate coordinate = static_cast<QnVectorSpaceMapper::Coordinate>(i);

        if(fromCamera.mapper(coordinate).isNull() && toCamera.mapper(coordinate).isNull())
            return false;
        if(fromCamera.mapper(coordinate).isNull())
            fromCamera.setMapper(coordinate, toCamera.mapper(coordinate));
        if(toCamera.mapper(coordinate).isNull())
            toCamera.setMapper(coordinate, fromCamera.mapper(coordinate));
    }

    *target = QnPtzSpaceMapper(fromCamera, toCamera, models);
    return true;
}
