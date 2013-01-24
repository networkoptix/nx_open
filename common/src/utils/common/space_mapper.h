#ifndef QN_SPACE_MAPPER_H
#define QN_SPACE_MAPPER_H

#include <boost/array.hpp>

#include <QtCore/QPair>
#include <QtCore/QVariant>

#include "interpolator.h"


// -------------------------------------------------------------------------- //
// QnScalarSpaceMapper
// -------------------------------------------------------------------------- //
class QnScalarSpaceMapper {
public:
    QnScalarSpaceMapper() {}

    QnScalarSpaceMapper(qreal logical0, qreal logical1, qreal physical0, qreal physical1, Qn::ExtrapolationMode extrapolationMode);

    QnScalarSpaceMapper(const QVector<QPointF> &logicalToPhysical, Qn::ExtrapolationMode extrapolationMode) {
        init(logicalToPhysical, extrapolationMode);
    }

    qreal logicalMinimum() const { return m_logicalMinimum; }
    qreal logicalMaximum() const { return m_logicalMaximum; }

    qreal physicalMinimum() const { return m_physicalMinimum; }
    qreal physicalMaximum() const { return m_physicalMaximum; }

    qreal logicalToPhysical(qreal logicalValue) const { return m_logicalToPhysical(logicalValue); }
    qreal physicalToLogical(qreal physicalValue) const { return m_physicalToLogical(physicalValue); }

    static QVariant serialize(const QnScalarSpaceMapper &value);
    static QnScalarSpaceMapper deserialize(const QVariant &value, bool *ok = NULL);

private:
    void init(const QVector<QPointF> &logicalToPhysical, Qn::ExtrapolationMode extrapolationMode);

private:
    QnInterpolator m_physicalToLogical, m_logicalToPhysical;
    qreal m_logicalMinimum, m_logicalMaximum, m_physicalMinimum, m_physicalMaximum;
};


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

    const QnScalarSpaceMapper &mapper(Coordinate coordinate) const {
        return m_mappers[coordinate];
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


#endif // QN_SPACE_MAPPER_H
