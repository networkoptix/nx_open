#ifndef QN_MATH_H
#define QN_MATH_H

#include <cmath>
#include <cfloat>
#include <cassert>
#include <limits>

#include <QtCore/QtGlobal>
#include <QtCore/qnumeric.h>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>

#include "fuzzy.h"


#ifndef M_E
#   define M_E 2.71828182845904523536
#endif

#ifndef M_LN2
#   define M_LN2 0.693147180559945309417
#endif

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif

#ifndef INT64_MAX
#   define INT64_MAX 0x7fffffffffffffffll
#endif

#ifndef INT64_MIN
#   define INT64_MIN 0x8000000000000000ll
#endif


inline bool qIsNaN(const QVector2D &vector) {
    return qIsNaN(vector.x()) || qIsNaN(vector.y());
}

inline bool qIsNaN(const QVector3D &vector) {
    return qIsNaN(vector.x()) || qIsNaN(vector.y()) || qIsNaN(vector.z());
}

inline bool qIsNaN(const QVector4D &vector) {
    return qIsNaN(vector.x()) || qIsNaN(vector.y()) || qIsNaN(vector.z()) || qIsNaN(vector.w());
}

template<class T> inline T qQNaN();
template<class T> inline T qSNaN();

#define QN_DEFINE_NAN_FUNCTION(TYPE, NAME, VALUE) template<> inline TYPE NAME<TYPE>() { return VALUE; }                           
QN_DEFINE_NAN_FUNCTION(float, qQNaN, std::numeric_limits<float>::quiet_NaN())
QN_DEFINE_NAN_FUNCTION(float, qSNaN, std::numeric_limits<float>::signaling_NaN())
QN_DEFINE_NAN_FUNCTION(double, qQNaN, std::numeric_limits<double>::quiet_NaN())
QN_DEFINE_NAN_FUNCTION(double, qSNaN, std::numeric_limits<double>::signaling_NaN())
#undef QN_DEFINE_NAN_FUNCTION

#define QN_DEFINE_COMPOSITE_NAN_FUNCTIONS(TYPE, SOURCE_TYPE, VALUE)             \
template<> inline TYPE qQNaN<TYPE>() { SOURCE_TYPE nan = qQNaN<SOURCE_TYPE>(); return VALUE; } \
template<> inline TYPE qSNaN<TYPE>() { SOURCE_TYPE nan = qSNaN<SOURCE_TYPE>(); return VALUE; }

QN_DEFINE_COMPOSITE_NAN_FUNCTIONS(QVector2D, qreal, QVector2D(nan, nan))
QN_DEFINE_COMPOSITE_NAN_FUNCTIONS(QVector3D, qreal, QVector3D(nan, nan, nan))
QN_DEFINE_COMPOSITE_NAN_FUNCTIONS(QVector4D, qreal, QVector4D(nan, nan, nan, nan))
#undef QN_DEFINE_COMPOSITE_NAN_FUNCTIONS


/**
 * \param value                         Value to check.
 * \param min                           Interval's left border.
 * \param max                           Interval's right border.
 * \returns                             Whether the given value lies in [min, max) interval.
 */
template<class T>
bool qBetween(const T &value, const T &min, const T &max) {
    return min <= value && value < max;
}


#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
/**
 * Converts the given 64-bit number from host byte order to network byte order.
 * 
 * \param x                             Value to convert, in host byte order.
 * \returns                             Converted value, in network byte order.
 */
inline quint64 htonll(quint64 x) {
    return
        ((((x) & 0xff00000000000000ULL) >> 56) | 
        (((x) & 0x00ff000000000000ULL) >> 40) | 
        (((x) & 0x0000ff0000000000ULL) >> 24) | 
        (((x) & 0x000000ff00000000ULL) >> 8) | 
        (((x) & 0x00000000ff000000ULL) << 8) | 
        (((x) & 0x0000000000ff0000ULL) << 24) | 
        (((x) & 0x000000000000ff00ULL) << 40) | 
        (((x) & 0x00000000000000ffULL) << 56));
}

/**
 * Converts the given 64-bit number from network byte order to host byte order.
 * 
 * \param x                             Value to convert, in network byte order.
 * \returns                             Converted value, in host byte order.
 */
inline quint64 ntohll(quint64 x) { return htonll(x); }

#else
inline quint64 htonll(quint64 x) { return x;}
inline quint64 ntohll(quint64 x)  { return x;}
#endif


/**
 * \param value
 * \returns                             Whether the given value is a power of 2.
 */
template<class T>
inline bool qIsPower2(T value) {
    return (value & (value - 1)) == 0;
}

/**
 * \param value                         Value to round up.
 * \param step                          Rounding step, must be power of 2.
 * \returns                             Rounded value.
 */
inline unsigned int qPower2Ceil(unsigned int value, int step) {
    DEBUG_CODE(assert(qIsPower2(step)));
    return ((value - 1) & ~(step - 1)) + step;
}

inline quint64 qPower2Ceil(quint64 value, int step) {
    DEBUG_CODE(assert(qIsPower2(step)));
    return ((value - 1) & ~(step - 1)) + step;
}

/**
 * \param value                         Value to round down.
 * \param step                          Rounding step, must be power of 2.
 * \returns                             Rounded value.
 */
inline unsigned int qPower2Floor(unsigned int value, int step) {
    DEBUG_CODE(assert(qIsPower2(step)));
    return value & ~(step - 1);
}

/**
 * Modulo function that never returns negative values.
 * 
 * \param l                             The dividend.
 * \param r                             The divisor.
 * \returns                             Non-negative modulo.
 */
template<class T>
T qMod(T l, T r) {
    return (l % r + r) % r;
}

inline float qMod(float l, float r) {
    float result = std::fmod(l, r);
    if(result < 0.0f)
        result += r;
    return result;
}

inline double qMod(double l, double r) {
    double result = std::fmod(l, r);
    if(result < 0.0)
        result += r;
    return result;
}

/**
 * \param value                         Value to round up.
 * \param step                          Rounding step, must be positive.
 * \returns                             Rounded value.
 */
template<class T>
T qCeil(T value, T step) {
    DEBUG_CODE(assert(step > 0));
    value = value + step - 1;
    return value - qMod(value, step);
}

/**
 * \param value                         Value to round down.
 * \param step                          Rounding step, must be positive.
 * \returns                             Rounded value.
 */
template<class T>
T qFloor(T value, T step) {
    DEBUG_CODE(assert(step > 0));
    return qCeil(value - step + 1, step);
}

/**
 * \param value                         Value to round.
 * \param step                          Rounding step, must be positive.
 * \returns                             Rounded value.
 */
template<class T>
T qRound(T value, T step) {
    DEBUG_CODE(assert(step > 0));
    return qCeil(value - step / 2, step);
}

/**
 * \returns                             Index of the leading 1 bit, counting the least significant bit at index 0.  
 *                                      (1 << IntegerLog2(x)) is a mask for the most significant bit of x.
 *                                      Result is undefined if input is zero.
 */
int qIntegerLog2(quint32 value);

#if defined(__GNUC__)
inline int qIntegerLog2(quint32 value) {
    return 31 - __builtin_clz(value);
}
#elif defined(_MSC_VER)
#   include <intrin.h>
#   pragma intrinsic(_BitScanReverse)
inline int qIntegerLog2(quint32 value) {
    unsigned long result;             /* MSVC intrinsic demands this type. */
    _BitScanReverse(&result, value);
    return result;
}
#else
inline int qIntegerLog2(quint32 value) {
    /* Default version using regular operations. Code taken from:
     * http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog */
    int result, shift;

    shift = (value > 0xFFFF) << 4;
    value >>= shift;
    result = shift;

    shift = (value > 0xFF) << 3;
    value >>= shift;
    result |= shift;

    shift = (value > 0xF) << 2;
    value >>= shift;
    result |= shift;

    shift = (value > 0x3) << 1;
    value >>= shift;
    result |= shift;

    result |= (value >> 1);

    return result;
}
#endif

#endif // QN_MATH_H
