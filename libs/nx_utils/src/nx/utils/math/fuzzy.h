// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <QtCore/QSizeF>
#include <QtCore/QtNumeric>
#include <QtGui/QMatrix4x4>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>

// -------------------------------------------------------------------------- //
// qFuzzyIsNull
// -------------------------------------------------------------------------- //
inline bool qFuzzyIsNull(const QVector2D &vector) noexcept
{
    return qFuzzyIsNull(vector.x()) && qFuzzyIsNull(vector.y());
}

inline bool qFuzzyIsNull(const QVector3D &vector) noexcept
{
    return qFuzzyIsNull(vector.x()) && qFuzzyIsNull(vector.y()) && qFuzzyIsNull(vector.z());
}

inline bool qFuzzyIsNull(const QVector4D &vector) noexcept
{
    return qFuzzyIsNull(vector.x()) && qFuzzyIsNull(vector.y()) && qFuzzyIsNull(vector.z()) && qFuzzyIsNull(vector.w());
}


// -------------------------------------------------------------------------- //
// qFuzzyEquals
// -------------------------------------------------------------------------- //
/* These numbers define default precision for qFloatDistance equality comparison. */
#define QN_FLOAT_FUZZY_EQUALS_PRECISION 84
#define QN_DOUBLE_FUZZY_EQUALS_PRECISION 4504

inline bool qFuzzyEquals(float l, float r) noexcept
{
    return qFuzzyCompare(l, r) || (qFuzzyIsNull(l) && qFuzzyIsNull(r));
}

inline bool qFuzzyEquals(double l, double r) noexcept
{
    return qFuzzyCompare(l, r) || (qFuzzyIsNull(l) && qFuzzyIsNull(r));
}

inline bool qFuzzyEquals(const QPointF &l, const QPointF &r) noexcept
{
    return qFuzzyEquals(l.x(), r.x()) && qFuzzyEquals(l.y(), r.y());
}

inline bool qFuzzyEquals(const QSizeF &l, const QSizeF &r) noexcept
{
    return qFuzzyEquals(l.width(), r.width()) && qFuzzyEquals(l.height(), r.height());
}

inline bool qFuzzyEquals(const QRectF &l, const QRectF &r) noexcept
{
    return
        qFuzzyEquals(l.x(), r.x()) &&
        qFuzzyEquals(l.y(), r.y()) &&
        qFuzzyEquals(l.width(), r.width()) &&
        qFuzzyEquals(l.height(), r.height());
}

inline bool qFuzzyEquals(const QVector2D &l, const QVector2D &r) noexcept
{
    return qFuzzyEquals(l.x(), r.x()) && qFuzzyEquals(l.y(), r.y());
}

inline bool qFuzzyEquals(const QVector3D &l, const QVector3D &r) noexcept
{
    return qFuzzyEquals(l.x(), r.x()) && qFuzzyEquals(l.y(), r.y()) && qFuzzyEquals(l.z(), r.z());
}

inline bool qFuzzyEquals(const QVector4D &l, const QVector4D &r) noexcept
{
    return qFuzzyEquals(l.x(), r.x()) && qFuzzyEquals(l.y(), r.y()) && qFuzzyEquals(l.w(), r.w());
}

inline bool qFuzzyEquals(const QMatrix4x4 &l, const QMatrix4x4 &r) noexcept
{
    auto lm = l.constData();
    auto rm = r.constData();
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (!qFuzzyEquals(lm[i * 4 + j], rm[i * 4 + j]))
                return false;
    return true;
}

// -------------------------------------------------------------------------- //
// Fuzzy border functions
// -------------------------------------------------------------------------- //
namespace QnFuzzyDetail {
template<class T>
inline bool qFuzzyBetween(const T &min, const T &value, const T &max) noexcept
{
    return (min <= value && value <= max) || qFuzzyEquals(value, min) || qFuzzyEquals(value, max);
}

} // namespace QnFuzzyDetail


/**
 * \param value                         Value to check.
 * \param min                           Interval's left border.
 * \param max                           Interval's right border.
 * \returns                             Whether the given value lies in [min, max] interval.
 */
inline bool qFuzzyBetween(double min, double value, double max) noexcept
{
    return QnFuzzyDetail::qFuzzyBetween(min, value, max);
}

inline bool qFuzzyBetween(float min, float value, float max) noexcept
{
    return QnFuzzyDetail::qFuzzyBetween(min, value, max);
}

float NX_UTILS_API qFuzzyFloor(float value);

double NX_UTILS_API qFuzzyFloor(double value);

float NX_UTILS_API qFuzzyCeil(float value);

double NX_UTILS_API qFuzzyCeil(double value);

bool NX_UTILS_API qFuzzyContains(const QRectF &rect, const QPointF &point);
