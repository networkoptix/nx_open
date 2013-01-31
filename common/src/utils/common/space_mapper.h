#ifndef QN_SPACE_MAPPER_H
#define QN_SPACE_MAPPER_H

#include <boost/array.hpp>

#include <QtCore/QVariant>
#include <QtCore/QStringList>
#include <QtCore/QMetaType>

#include "interpolator.h"


// -------------------------------------------------------------------------- //
// QnScalarSpaceMapper
// -------------------------------------------------------------------------- //
class QnScalarSpaceMapper {
public:
    QnScalarSpaceMapper() {
        init(QVector<QPointF>(), m_logicalToPhysical.extrapolationMode());
    }

    QnScalarSpaceMapper(qreal logical0, qreal logical1, qreal physical0, qreal physical1, Qn::ExtrapolationMode extrapolationMode);

    QnScalarSpaceMapper(const QVector<QPointF> &logicalToPhysical, Qn::ExtrapolationMode extrapolationMode) {
        init(logicalToPhysical, extrapolationMode);
    }

    bool isNull() const {
        return m_logicalToPhysical.isNull();
    }

    qreal logicalMinimum() const { return m_logicalMinimum; }
    qreal logicalMaximum() const { return m_logicalMaximum; }

    qreal physicalMinimum() const { return m_physicalMinimum; }
    qreal physicalMaximum() const { return m_physicalMaximum; }

    qreal logicalToPhysical(qreal logicalValue) const { return m_logicalToPhysical(logicalValue); }
    qreal physicalToLogical(qreal physicalValue) const { return m_physicalToLogical(physicalValue); }

    const QnInterpolator &logicalToPhysical() const { return m_logicalToPhysical; }
    const QnInterpolator &physicalToLogical() const { return m_physicalToLogical; }

private:
    void init(const QVector<QPointF> &logicalToPhysical, Qn::ExtrapolationMode extrapolationMode);

private:
    QnInterpolator m_physicalToLogical, m_logicalToPhysical;
    qreal m_logicalMinimum, m_logicalMaximum, m_physicalMinimum, m_physicalMaximum;
};

void serialize(const QnScalarSpaceMapper &value, QVariant *target);
bool deserialize(const QVariant &value, QnScalarSpaceMapper *target);


// -------------------------------------------------------------------------- //
// QnVectorSpaceMapper
// -------------------------------------------------------------------------- //
class QnVectorSpaceMapper {
public:
    enum Coordinate {
        X,
        Y,
        Z,
        CoordinateCount
    };

    QnVectorSpaceMapper() {}

    QnVectorSpaceMapper(const QnScalarSpaceMapper &xMapper, const QnScalarSpaceMapper &yMapper, const QnScalarSpaceMapper &zMapper) {
        m_mappers[X] = xMapper;
        m_mappers[Y] = yMapper;
        m_mappers[Z] = zMapper;
    }

    bool isNull() const {
        return m_mappers[X].isNull() && m_mappers[Y].isNull() && m_mappers[Z].isNull();
    }

    const QnScalarSpaceMapper &mapper(Coordinate coordinate) const {
        return m_mappers[coordinate];
    }

    void setMapper(Coordinate coordinate, const QnScalarSpaceMapper &mapper) {
        m_mappers[coordinate] = mapper;
    }

    QVector3D logicalToPhysical(const QVector3D &logicalValue) const { 
        return QVector3D(
            m_mappers[X].logicalToPhysical(logicalValue.x()),
            m_mappers[Y].logicalToPhysical(logicalValue.y()),
            m_mappers[Z].logicalToPhysical(logicalValue.z())
        );
    }

    QVector3D physicalToLogical(const QVector3D &physicalValue) const { 
        return QVector3D(
            m_mappers[X].physicalToLogical(physicalValue.x()),
            m_mappers[Y].physicalToLogical(physicalValue.y()),
            m_mappers[Z].physicalToLogical(physicalValue.z())
        );
    }

private:
    boost::array<QnScalarSpaceMapper, CoordinateCount> m_mappers;
};

void serialize(const QnVectorSpaceMapper &value, QVariant *target);
bool deserialize(const QVariant &value, QnVectorSpaceMapper *target);


// -------------------------------------------------------------------------- //
// QnPtzSpaceMapper
// -------------------------------------------------------------------------- //
class QnPtzSpaceMapper {
public:
    QnPtzSpaceMapper() {}
    QnPtzSpaceMapper(const QnVectorSpaceMapper &mapper, const QStringList &models): m_fromCamera(mapper), m_toCamera(mapper), m_models(models) {}
    QnPtzSpaceMapper(const QnVectorSpaceMapper &fromCamera, const QnVectorSpaceMapper &toCamera, const QStringList &models): m_fromCamera(fromCamera), m_toCamera(toCamera), m_models(models) {}

    bool isNull() const {
        return m_models.isEmpty() && m_fromCamera.isNull() && m_toCamera.isNull();
    }

    const QnVectorSpaceMapper &fromCamera() const {
        return m_fromCamera;
    }

    const QnVectorSpaceMapper &toCamera() const {
        return m_toCamera;
    }

    const QStringList &models() const {
        return m_models;
    }

private:
    QStringList m_models;
    QnVectorSpaceMapper m_fromCamera, m_toCamera;
};

void serialize(const QnPtzSpaceMapper &value, QVariant *target);
bool deserialize(const QVariant &value, QnPtzSpaceMapper *target);

Q_DECLARE_METATYPE(QnPtzSpaceMapper);


#endif // QN_SPACE_MAPPER_H
