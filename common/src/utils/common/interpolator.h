#ifndef QN_INTERPOLATOR_H
#define QN_INTERPOLATOR_H

#include <cassert>

#include <functional>

#include <QtCore/QVector>

#include "math.h"

namespace Qn {
    enum ExtrapolationMode {
        ConstantExtrapolation,
        LinearExtrapolation,
        PeriodicExtrapolation
    };
}

template<class T>
class QnInterpolator: public std::unary_function<T, T> {
public:
    struct Point {
        Point() {}
        Point(const T &x, const T &y): x(x), y(y) {}
        T x, y;
    };

    QnInterpolator(): m_extrapolationMode(Qn::ConstantExtrapolation), m_ready(false) {}

    void addPoint(const T &x, const T &y) {
        m_points.push_back(Point(x, y));
        m_ready = false;
    }

    void clear() {
        m_points.clear();
        m_ready = false;
    }

    const QVector<Point> &points() {
        ensureReady();

        return m_points;
    }

    const Point &point(int index) {
        return m_points[index];
    }

    T operator()(const T &x) const{
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
        bool operator()(const Point &l, const Point &r) {
            return l.x < r.x;
        }
    };

    void ensureReady() const {
        if(m_ready)
            return;

        qSort(m_points.begin(), m_points.end(), PointLess());
        m_ready = true;
    }

    T valueInternal(const T &x, Qn::ExtrapolationMode extrapolationMode) const {
        typename QVector<Point>::const_iterator pos = qLowerBound(m_points.begin(), m_points.end(), Point(x, T()), PointLess());

        if(pos == m_points.begin()) {
            return extrapolateStart(x, extrapolationMode);
        } else if(pos == m_points.end()) {
            return extrapolateEnd(x, extrapolationMode);
        } else {
            return interpolate(*(pos - 1), *pos, x);
        }
    }

    T interpolate(const Point &a, const Point &b, const T &x) const {
        return (a.y * (b.x - x) + b.y * (x - a.x)) / (b.x - a.x);
    }

    T extrapolateStart(const T &x, Qn::ExtrapolationMode extrapolationMode) const {
        if(m_points.size() < 2)
            return extrapolateNoData();

        switch(extrapolationMode) {
        case Qn::ConstantExtrapolation:
            return m_points.front().y;
        case Qn::LinearExtrapolation:
            return interpolate(*m_points.begin(), *(m_points.begin() + 1), x);
        case Qn::PeriodicExtrapolation:
            return extrapolatePeriodic(x);
        default:
            assert(false);
            return T();
        }
    }

    T extrapolateEnd(const T &x, Qn::ExtrapolationMode extrapolationMode) const {
        if(m_points.size() < 2)
            return extrapolateNoData();

        switch(extrapolationMode) {
        case Qn::ConstantExtrapolation:
            return m_points.back().y;
        case Qn::LinearExtrapolation:
            return interpolate(*(m_points.end() - 2), *(m_points.end() - 1), x);
        case Qn::PeriodicExtrapolation:
            return extrapolatePeriodic(x);
        default:
            assert(false);
            return T();
        }
    }

    T extrapolatePeriodic(const T &x) const {
        T start = m_points.front().x;
        T end = m_points.back().x;

        /* Pass linear extrapolation here so that we don't end up in 
         * infinite recursion due to FP precision errors. */
        return valueInternal(start + qMod(x - start, end - start), Qn::LinearExtrapolation);
    }

    T extrapolateNoData() const {
        if(m_points.isEmpty()) {
            return T();
        } else {
            return m_points.front().y;
        }
    }

private:
    Qn::ExtrapolationMode m_extrapolationMode;
    mutable bool m_ready;
    mutable QVector<Point> m_points;
};


#endif // QN_INTERPOLATOR_H
