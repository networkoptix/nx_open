#ifndef QN_PTZ_MAPPER_H
#define QN_PTZ_MAPPER_H

#include <cassert>

#include <utils/common/functors.h>

class QnPhysicalValueMapper {
public:
    QnPhysicalValueMapper() {}

    QnPhysicalValueMapper(qreal logicalMinimum, qreal logicalMaximum, qreal physicalAtMinimum, qreal physicalAtMaximum) {
        m_physicalToLogical = QnBoundedLinearFunction(
            QnLinearFunction(physicalAtMinimum, logicalMinimum, physicalAtMaximum, logicalMaximum),
            logicalMinimum,
            logicalMaximum
        );

        m_logicalToPhysical = QnBoundedLinearFunction(
            QnLinearFunction(physicalAtMinimum, logicalMinimum, physicalAtMaximum, logicalMaximum),
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


class QnPtzMapper {
public:
    enum Coordinate {
        X,
        Y,
        Zoom,
        CoordinateCount
    };

    QnPtzMapper() {}

    QnPtzMapper(const QnPhysicalValueMapper &xMapper, const QnPhysicalValueMapper &yMapper, const QnPhysicalValueMapper &zoomMapper) {
        m_mappers[X] = xMapper;
        m_mappers[Y] = yMapper;
        m_mappers[Zoom] = zoomMapper;
    }

    const QnPhysicalValueMapper &mapper(Coordinate coordinate) const {
        assert(coordinate >= 0 && coordinate < CoordinateCount);

        return m_mappers[coordinate];
    }

    const QnPhysicalValueMapper &xMapper() const {
        return mapper(X);
    }

    const QnPhysicalValueMapper &yMapper() const {
        return mapper(Y);
    }

    const QnPhysicalValueMapper &zoomMapper() const {
        return mapper(Zoom);
    }

private:
    QnPhysicalValueMapper m_mappers[CoordinateCount];
};


#endif // QN_PTZ_MAPPER_H
