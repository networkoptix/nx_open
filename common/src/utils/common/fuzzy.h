#ifndef QN_FUZZY_H
#define QN_FUZZY_H 

#include <QPointF>
#include <QSizeF>
#include <QRectF>

inline bool qFuzzyIsNull(const QPointF &p) {
    return ::qFuzzyIsNull(p.x()) && ::qFuzzyIsNull(p.y());
}

inline bool qFuzzyIsNull(const QSizeF &s) {
    return ::qFuzzyIsNull(s.width()) && ::qFuzzyIsNull(s.height());
}

inline bool qFuzzyCompare(const QPointF &l, const QPointF &r) {
    return ::qFuzzyCompare(l.x(), r.x()) && ::qFuzzyCompare(l.y(), r.y());
}

inline bool qFuzzyCompare(const QSizeF &l, const QSizeF &r) {
    return ::qFuzzyCompare(l.width(), r.width()) && ::qFuzzyCompare(l.height(), r.height());
}

inline bool qFuzzyCompare(const QRectF &l, const QRectF &r) {
    return 
        ::qFuzzyCompare(l.x(), r.x()) && 
        ::qFuzzyCompare(l.y(), r.y()) && 
        ::qFuzzyCompare(l.width(), r.width()) &&
        ::qFuzzyCompare(l.height(), r.height());
}

/**
 * \param value                         Value to check.
 * \param min                           Interval's left border.
 * \param max                           Interval's right border.
 * \returns                             Whether the given value lies in [min, max] interval, 
 */
inline bool qFuzzyBetween(double value, double min, double max, double precision = 0.000000000001) {
    double localPrecision = precision * qMax(qAbs(min), qAbs(max));

    return min - localPrecision <= value && value <= max + localPrecision;
}

float qFuzzyFloor(float value);

double qFuzzyFloor(double value);

float qFuzzyCeil(float value);

double qFuzzyCeil(double value);

bool qFuzzyContains(const QRectF &rect, const QPointF &point);

#endif // QN_FUZZY_H
