#ifndef QN_PTZ_MAPPER_H
#define QN_PTZ_MAPPER_H

#include <cassert>

#include <utils/common/functors.h>

class QnScalarSpaceMapper {
public:
    QnScalarSpaceMapper() {}

    QnScalarSpaceMapper(qreal logicalMinimum, qreal logicalMaximum, qreal physicalAtMinimum, qreal physicalAtMaximum) {
        m_physicalToLogical = QnBoundedLinearFunction(
            QnLinearFunction(physicalAtMinimum, logicalMinimum, physicalAtMaximum, logicalMaximum),
            logicalMinimum,
            logicalMaximum
        );

        m_logicalToPhysical = QnBoundedLinearFunction(
            QnLinearFunction(logicalMinimum, physicalAtMinimum, logicalMaximum, physicalAtMaximum),
            qMin(physicalAtMinimum, physicalAtMaximum),
            qMax(physicalAtMinimum, physicalAtMaximum)
        );
    }

    qreal logicalMinimum() const { return m_physicalToLogical.minimum(); }
    qreal logicalMaximum() const { return m_physicalToLogical.maximum(); }

    qreal physicalMinimum() const { return m_logicalToPhysical.minimum(); }
    qreal physicalMaximum() const { return m_logicalToPhysical.minimum(); }

    qreal logicalToPhysical(qreal logicalValue) const { return m_logicalToPhysical(logicalValue); }
    qreal physicalToLogical(qreal physicalValue) const { return m_physicalToLogical(physicalValue); }

private:
    QnBoundedLinearFunction m_physicalToLogical, m_logicalToPhysical;
};


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
        assert(coordinate >= 0 && coordinate < CoordinateCount);

        return m_mappers[coordinate];
    }

    const QnScalarSpaceMapper &xMapper() const {
        return mapper(X);
    }

    const QnScalarSpaceMapper &yMapper() const {
        return mapper(Y);
    }

    const QnScalarSpaceMapper &zMapper() const {
        return mapper(Z);
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
    QnScalarSpaceMapper m_mappers[CoordinateCount];
};


#endif // QN_PTZ_MAPPER_H
