#ifndef QN_SPACE_MAPPER_H
#define QN_SPACE_MAPPER_H

#include <cassert>

#include <QtCore/QPair>

#include "interpolator.h"

class QnScalarSpaceMapper {
public:
    QnScalarSpaceMapper() {}

    QnScalarSpaceMapper(qreal logical0, qreal logical1, qreal physical0, qreal physical1, Qn::ExtrapolationMode extrapolationMode) {
        QVector<QPair<qreal, qreal> > logicalToPhysical;
        logicalToPhysical.push_back(qMakePair(logical0, physical0));
        logicalToPhysical.push_back(qMakePair(logical1, physical1));
        init(logicalToPhysical, extrapolationMode);
    }

    QnScalarSpaceMapper(const QVector<QPair<qreal, qreal> > &logicalToPhysical, Qn::ExtrapolationMode extrapolationMode) {
        init(logicalToPhysical, extrapolationMode);
    }

    qreal logicalMinimum() const { return m_logicalMinimum; }
    qreal logicalMaximum() const { return m_logicalMaximum; }

    qreal physicalMinimum() const { return m_physicalMinimum; }
    qreal physicalMaximum() const { return m_physicalMaximum; }

    qreal logicalToPhysical(qreal logicalValue) const { return m_logicalToPhysical(logicalValue); }
    qreal physicalToLogical(qreal physicalValue) const { return m_physicalToLogical(physicalValue); }

private:
    void init(const QVector<QPair<qreal, qreal> > &logicalToPhysical, Qn::ExtrapolationMode extrapolationMode) {
        typedef QPair<qreal, qreal> Point; // TODO: #C++0x replace with auto
        foreach(const Point &v, logicalToPhysical) {
            m_logicalToPhysical.addPoint(v.first, v.second);
            m_physicalToLogical.addPoint(v.second, v.first);
        }
        m_logicalToPhysical.setExtrapolationMode(extrapolationMode);
        m_physicalToLogical.setExtrapolationMode(extrapolationMode);

        qreal logicalA, logicalB, physicalA, physicalB;
        if(m_logicalToPhysical.points().empty()) {
            logicalA = logicalB = m_physicalToLogical(0.0);
            physicalA = physicalB = m_logicalToPhysical(0.0);
        } else {
            int last = logicalToPhysical.size() - 1;

            logicalA = m_logicalToPhysical.point(0).x;
            logicalB = m_logicalToPhysical.point(last).x;
            physicalA = m_physicalToLogical.point(0).x;
            physicalB = m_physicalToLogical.point(last).x;
        }

        m_logicalMinimum = qMin(logicalA, logicalB);
        m_logicalMaximum = qMax(logicalA, logicalB);
        m_physicalMinimum = qMin(physicalA, physicalB);
        m_physicalMaximum = qMax(physicalA, physicalB);
    }

private:
    QnInterpolator<qreal> m_physicalToLogical, m_logicalToPhysical;
    qreal m_logicalMinimum, m_logicalMaximum, m_physicalMinimum, m_physicalMaximum;
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


#endif // QN_SPACE_MAPPER_H
