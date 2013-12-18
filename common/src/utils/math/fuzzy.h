#ifndef QN_FUZZY_H
#define QN_FUZZY_H 

#include <QtCore/QPointF>
#include <QtCore/QSizeF>
#include <QtCore/QRectF>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>

// TODO: #Elric #5.3 remove
quint32 qFloatDistance(float a, float b);
quint64 qFloatDistance(double a, double b);

// -------------------------------------------------------------------------- //
// qFuzzyIsNull
// -------------------------------------------------------------------------- //
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

inline bool qFuzzyIsNull(const QVector2D &vector) {
    return qFuzzyIsNull(vector.x()) && qFuzzyIsNull(vector.y());
}

inline bool qFuzzyIsNull(const QVector3D &vector) {
    return qFuzzyIsNull(vector.x()) && qFuzzyIsNull(vector.y()) && qFuzzyIsNull(vector.z());
}

inline bool qFuzzyIsNull(const QVector4D &vector) {
    return qFuzzyIsNull(vector.x()) && qFuzzyIsNull(vector.y()) && qFuzzyIsNull(vector.z()) && qFuzzyIsNull(vector.w());
}


// -------------------------------------------------------------------------- //
// qFuzzyEquals
// -------------------------------------------------------------------------- //
/* These numbers define precision of qFuzzyCompare. */
#define QN_FLOAT_FUZZY_EQUALS_PRECISION 84
#define QN_DOUBLE_FUZZY_EQUALS_PRECISION 4504

// TODO: #Elric deprecate qFuzzyCompare

inline bool qFuzzyEquals(float l, float r, quint32 distance = QN_FLOAT_FUZZY_EQUALS_PRECISION) {
    return qFloatDistance(l, r) < distance;
}

inline bool qFuzzyEquals(double l, double r, quint64 distance = QN_DOUBLE_FUZZY_EQUALS_PRECISION) {
    return qFloatDistance(l, r) < distance;
}

inline bool qFuzzyEquals(const QPointF &l, const QPointF &r) {
    return qFuzzyEquals(l.x(), r.x()) && qFuzzyEquals(l.y(), r.y());
}

inline bool qFuzzyEquals(const QSizeF &l, const QSizeF &r) {
    return qFuzzyEquals(l.width(), r.width()) && qFuzzyEquals(l.height(), r.height());
}

inline bool qFuzzyEquals(const QRectF &l, const QRectF &r) {
    return 
        qFuzzyEquals(l.x(), r.x()) && 
        qFuzzyEquals(l.y(), r.y()) && 
        qFuzzyEquals(l.width(), r.width()) &&
        qFuzzyEquals(l.height(), r.height());
}


// -------------------------------------------------------------------------- //
// Fuzzy border functions
// -------------------------------------------------------------------------- //

// TODO: #Elric clean this part up

namespace QnFuzzyDetail {
    template<class T>
    inline bool qFuzzyBetween(const T &value, const T &min, const T &max) {
        return (min <= value && value <= max) || qFuzzyEquals(value, min) || qFuzzyEquals(value, max);
    }

} // namespace QnFuzzyDetail


/**
 * \param value                         Value to check.
 * \param min                           Interval's left border.
 * \param max                           Interval's right border.
 * \returns                             Whether the given value lies in [min, max] interval.
 */
inline bool qFuzzyBetween(double value, double min, double max) {
    return QnFuzzyDetail::qFuzzyBetween(value, min, max);
}

inline bool qFuzzyBetween(float value, float min, float max) {
    return QnFuzzyDetail::qFuzzyBetween(value, min, max);
}

float qFuzzyFloor(float value);

double qFuzzyFloor(double value);

float qFuzzyCeil(float value);

double qFuzzyCeil(double value);

bool qFuzzyContains(const QRectF &rect, const QPointF &point);

#endif // QN_FUZZY_H
