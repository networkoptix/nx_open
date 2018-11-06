#ifndef QN_INTERPOLATOR_H
#define QN_INTERPOLATOR_H

#include <cassert>
#include <functional>

#include <common/common_globals.h>

#include <QtCore/QVector>

#include "math.h"
#include "linear_combination.h"


template<class T>
class QnInterpolator {
public:
    typedef QPair<T, T> point_type;

    explicit QnInterpolator(Qn::ExtrapolationMode extrapolationMode = Qn::ConstantExtrapolation):
        m_extrapolationMode(extrapolationMode)
    {}

    QnInterpolator(const QVector<point_type> &points, Qn::ExtrapolationMode extrapolationMode = Qn::ConstantExtrapolation):
        m_extrapolationMode(extrapolationMode)
    {
        setPoints(points);
    }

    bool isNull() const {
        return m_points.isEmpty() && m_extrapolationMode == Qn::ConstantExtrapolation;
    }

    const QVector<point_type> &points() const {
        return m_points;
    }

    const point_type &point(int index) {
        return m_points[index];
    }

    void setPoints(const QVector<point_type> &points) {
        m_points = points;

        std::sort(m_points.begin(), m_points.end(), PointLess());
    }

    T operator()(const T &x) const{
        return valueInternal(x, m_extrapolationMode);
    }

    Qn::ExtrapolationMode extrapolationMode() const {
        return m_extrapolationMode;
    }

    void setExtrapolationMode(Qn::ExtrapolationMode extrapolationMode) {
        NX_ASSERT(extrapolationMode == Qn::ConstantExtrapolation || extrapolationMode == Qn::LinearExtrapolation || extrapolationMode == Qn::PeriodicExtrapolation);

        m_extrapolationMode = extrapolationMode;
    }

protected:
    struct PointLess {
        bool operator()(const point_type &l, const point_type &r) {
            return l.first < r.first;
        }
    };

    T valueInternal(const T &x, Qn::ExtrapolationMode extrapolationMode) const {
        typename QVector<point_type>::const_iterator pos = std::lower_bound(m_points.begin(), m_points.end(), point_type(x, T()), PointLess());

        if(pos == m_points.begin()) {
            return extrapolateStart(x, extrapolationMode);
        } else if(pos == m_points.end()) {
            return extrapolateEnd(x, extrapolationMode);
        } else {
            return interpolate(*(pos - 1), *pos, x);
        }
    }

    T interpolate(const point_type &a, const point_type &b, const T &x) const {
        const T &divisor = b.first - a.first;
        return linearCombine((b.first - x) / divisor, a.second, (x - a.first) / divisor, b.second);
    }

    T extrapolateStart(const T &x, Qn::ExtrapolationMode extrapolationMode) const {
        if(m_points.size() < 2)
            return extrapolateNoData();

        switch(extrapolationMode) {
        case Qn::ConstantExtrapolation:
            return m_points.front().second;
        case Qn::LinearExtrapolation:
            return interpolate(*m_points.begin(), *(m_points.begin() + 1), x);
        case Qn::PeriodicExtrapolation:
            return extrapolatePeriodic(x);
        default:
            NX_ASSERT(false);
            return 0.0;
        }
    }

    T extrapolateEnd(const T &x, Qn::ExtrapolationMode extrapolationMode) const {
        if(m_points.size() < 2)
            return extrapolateNoData();

        switch(extrapolationMode) {
        case Qn::ConstantExtrapolation:
            return m_points.back().second;
        case Qn::LinearExtrapolation:
            return interpolate(*(m_points.end() - 2), *(m_points.end() - 1), x);
        case Qn::PeriodicExtrapolation:
            return extrapolatePeriodic(x);
        default:
            NX_ASSERT(false);
            return 0.0;
        }
    }

    T extrapolatePeriodic(const T &x) const {
        T start = m_points.front().first;
        T end = m_points.back().first;

        /* Pass linear extrapolation here so that we don't end up in
         * infinite recursion due to FP precision errors. */
        return valueInternal(start + qMod(x - start, end - start), Qn::LinearExtrapolation);
    }

    T extrapolateNoData() const {
        if(m_points.isEmpty()) {
            return T();
        } else {
            return m_points.front().second;
        }
    }

private:
    Qn::ExtrapolationMode m_extrapolationMode;
    QVector<point_type> m_points;
};


#endif // QN_INTERPOLATOR_H
