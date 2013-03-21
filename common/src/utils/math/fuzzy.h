#ifndef QN_FUZZY_H
#define QN_FUZZY_H 

#include <QtCore/QPointF>
#include <QtCore/QSizeF>
#include <QtCore/QRectF>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>

#define QN_DEFINE_INTEGER_FUZZY_IS_NULL(TYPE)                                   \
inline bool qFuzzyIsNull(TYPE p) { return p == 0; }

QN_DEFINE_INTEGER_FUZZY_IS_NULL(short)
QN_DEFINE_INTEGER_FUZZY_IS_NULL(unsigned short)
QN_DEFINE_INTEGER_FUZZY_IS_NULL(int)
QN_DEFINE_INTEGER_FUZZY_IS_NULL(unsigned int)
QN_DEFINE_INTEGER_FUZZY_IS_NULL(long)
QN_DEFINE_INTEGER_FUZZY_IS_NULL(unsigned long)
QN_DEFINE_INTEGER_FUZZY_IS_NULL(long long)
QN_DEFINE_INTEGER_FUZZY_IS_NULL(unsigned long long)
#undef QN_DEFINE_INTEGER_FUZZY_IS_NULL

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

inline bool qFuzzyIsNull(const QVector2D &vector) {
    return qFuzzyIsNull(vector.x()) && qFuzzyIsNull(vector.y());
}

inline bool qFuzzyIsNull(const QVector3D &vector) {
    return qFuzzyIsNull(vector.x()) && qFuzzyIsNull(vector.y()) && qFuzzyIsNull(vector.z());
}

inline bool qFuzzyIsNull(const QVector4D &vector) {
    return qFuzzyIsNull(vector.x()) && qFuzzyIsNull(vector.y()) && qFuzzyIsNull(vector.z()) && qFuzzyIsNull(vector.w());
}


/**
 * \param value                         Value to check.
 * \param min                           Interval's left border.
 * \param max                           Interval's right border.
 * \returns                             Whether the given value lies in [min, max] interval.
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
