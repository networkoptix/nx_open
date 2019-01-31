#pragma once

#include <limits>

#include <QtCore/QtGlobal>

class QnLinearFunction
{
public:
    QnLinearFunction():
        m_a(1.0),
        m_b(0.0)
    {
    }

    QnLinearFunction(qreal a, qreal b):
        m_a(a),
        m_b(b)
    {
    }

    QnLinearFunction(qreal x0, qreal y0, qreal x1, qreal y1)
    {
        m_a = (y1 - y0) / (x1 - x0);
        m_b = y0 - m_a * x0;
    }

    qreal operator()(qreal value) const
    {
        return m_a * value + m_b;
    }

    QnLinearFunction inversed() const
    {
        return QnLinearFunction(1.0 / m_a, -m_b / m_a);
    }

private:
    qreal m_a, m_b;
};

class QnBoundedLinearFunction
{
public:
    QnBoundedLinearFunction():
        m_min(-std::numeric_limits<qreal>::max()),
        m_max(std::numeric_limits<qreal>::max())
    {
    }

    QnBoundedLinearFunction(const QnLinearFunction& function, qreal min, qreal max):
        m_function(function),
        m_min(min),
        m_max(max)
    {
    }

    qreal operator()(qreal value) const
    {
        return qBound(m_min, m_function(value), m_max);
    }

    qreal minimum() const
    {
        return m_min;
    }

    qreal maximum() const
    {
        return m_max;
    }

private:
    QnLinearFunction m_function;
    qreal m_min, m_max;
};
