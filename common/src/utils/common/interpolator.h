#ifndef QN_INTERPOLATOR_H
#define QN_INTERPOLATOR_H

#include <cassert>
#include <functional>

#include <common/common_globals.h>

#include <QtCore/QVector>
#include <QtCore/QPointF>

#include "math.h"

class QnInterpolator: public std::unary_function<qreal, qreal> {
public:
    QnInterpolator(): 
        m_extrapolationMode(Qn::ConstantExtrapolation), 
        m_ready(false) 
    {}

    bool isNull() const {
        return m_points.isEmpty() && m_extrapolationMode == Qn::ConstantExtrapolation;
    }

    void addPoint(qreal x, qreal y) {
        addPoint(QPointF(x, y));
    }

    void addPoint(const QPointF &point) {
        m_points.push_back(point);
        m_ready = false;
    }

    const QVector<QPointF> &points() const {
        ensureReady();

        return m_points;
    }

    const QPointF &point(int index) {
        return m_points[index];
    }

    qreal operator()(qreal x) const{
        ensureReady();

        return valueInternal(x, m_extrapolationMode);
    }

    Qn::ExtrapolationMode extrapolationMode() const {
        return m_extrapolationMode;
    }

    void setExtrapolationMode(Qn::ExtrapolationMode extrapolationMode) {
        assert(extrapolationMode == Qn::ConstantExtrapolation || extrapolationMode == Qn::LinearExtrapolation || extrapolationMode == Qn::PeriodicExtrapolation);

        m_extrapolationMode = extrapolationMode;
    }

protected:
    struct PointLess {
        bool operator()(const QPointF &l, const QPointF &r) {
            return l.x() < r.x();
        }
    };

    void ensureReady() const {
        if(m_ready)
            return;

        qSort(m_points.begin(), m_points.end(), PointLess());
        m_ready = true;
    }

    qreal valueInternal(qreal x, Qn::ExtrapolationMode extrapolationMode) const {
        QVector<QPointF>::const_iterator pos = qLowerBound(m_points.begin(), m_points.end(), QPointF(x, 0.0), PointLess());

        if(pos == m_points.begin()) {
            return extrapolateStart(x, extrapolationMode);
        } else if(pos == m_points.end()) {
            return extrapolateEnd(x, extrapolationMode);
        } else {
            return interpolate(*(pos - 1), *pos, x);
        }
    }

    qreal interpolate(const QPointF &a, const QPointF &b, qreal x) const {
        return (a.y() * (b.x() - x) + b.y() * (x - a.x())) / (b.x() - a.x());
    }

    qreal extrapolateStart(qreal x, Qn::ExtrapolationMode extrapolationMode) const {
        if(m_points.size() < 2)
            return extrapolateNoData();

        switch(extrapolationMode) {
        case Qn::ConstantExtrapolation:
            return m_points.front().y();
        case Qn::LinearExtrapolation:
            return interpolate(*m_points.begin(), *(m_points.begin() + 1), x);
        case Qn::PeriodicExtrapolation:
            return extrapolatePeriodic(x);
        default:
            assert(false);
            return 0.0;
        }
    }

    qreal extrapolateEnd(qreal x, Qn::ExtrapolationMode extrapolationMode) const {
        if(m_points.size() < 2)
            return extrapolateNoData();

        switch(extrapolationMode) {
        case Qn::ConstantExtrapolation:
            return m_points.back().y();
        case Qn::LinearExtrapolation:
            return interpolate(*(m_points.end() - 2), *(m_points.end() - 1), x);
        case Qn::PeriodicExtrapolation:
            return extrapolatePeriodic(x);
        default:
            assert(false);
            return 0.0;
        }
    }

    qreal extrapolatePeriodic(qreal x) const {
        qreal start = m_points.front().x();
        qreal end = m_points.back().x();

        /* Pass linear extrapolation here so that we don't end up in 
         * infinite recursion due to FP precision errors. */
        return valueInternal(start + qMod(x - start, end - start), Qn::LinearExtrapolation);
    }

    qreal extrapolateNoData() const {
        if(m_points.isEmpty()) {
            return 0.0;
        } else {
            return m_points.front().y();
        }
    }

private:
    Qn::ExtrapolationMode m_extrapolationMode;
    mutable bool m_ready;
    mutable QVector<QPointF> m_points;
};


#endif // QN_INTERPOLATOR_H
