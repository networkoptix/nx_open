#ifndef QN_MATH_H
#define QN_MATH_H

#include <cmath>
#include <cfloat>
#include <cassert>
#include <limits>

#include <QtCore/QtEndian>
#include <QtCore/QtGlobal>
#include <QtCore/QtNumeric>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>

#include <nx/utils/compiler_options.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

namespace QnMathDetail {
    using ::qFuzzyIsNull;

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
} // namespace QnMathDetail

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
 * \param min                           Interval's left border.
 * \param value                         Value to check.
 * \param max                           Interval's right border.
 * \returns                             Whether the given value lies in [min, max) interval.
 */
template<class T>
bool qBetween(const T &min, const T &value, const T &max) {
    return min <= value && value < max;
}

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
    DEBUG_CODE(NX_ASSERT(qIsPower2(step)));
    return ((value - 1) & ~(step - 1)) + step;
}

inline quint64 qPower2Ceil(quint64 value, int step) {
    DEBUG_CODE(NX_ASSERT(qIsPower2(step)));
    return ((value - 1) & ~(step - 1)) + step;
}

/**
 * \param value                         Value to round down.
 * \param step                          Rounding step, must be power of 2.
 * \returns                             Rounded value.
 */
inline unsigned int qPower2Floor(unsigned int value, int step) {
    DEBUG_CODE(NX_ASSERT(qIsPower2(step)));
    return value & ~(step - 1);
}

/**
* \param value                         Value to round to the nearest number that is power of 2.
* \param step                          Rounding step, must be power of 2.
* \returns                             Rounded value.
*/
inline unsigned int qPower2Round(unsigned int value, int step)
{
    DEBUG_CODE(NX_ASSERT(qIsPower2(step)));
    return qPower2Floor(value + step / 2, step);
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
template<class T, class Step>
T qCeil(T value, Step step) {
    DEBUG_CODE(NX_ASSERT(step > 0));
    T mod = qMod(value, static_cast<T>(step));
    return QnMathDetail::qFuzzyIsNull(mod) ? value : static_cast<T>(value - mod + step);
}

/**
 * \param value                         Value to round down.
 * \param step                          Rounding step, must be positive.
 * \returns                             Rounded value.
 */
template<class T, class Step>
T qFloor(T value, Step step) {
    DEBUG_CODE(NX_ASSERT(step > 0));
    return value - qMod(value, static_cast<T>(step));
}

/**
 * \param value                         Value to round.
 * \param step                          Rounding step, must be positive.
 * \returns                             Rounded value.
 */
template<class T, class Step>
T qRound(T value, Step step) {
    DEBUG_CODE(NX_ASSERT(step > 0));
    return qFloor(value + static_cast<T>(step) / 2, step);
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

/* Overloads for double-float interop. */

inline double qMod(double l, float r) {
    return qMod(l, static_cast<double>(r));
}

inline double qMod(float l, double r) {
    return qMod(static_cast<double>(l), r);
}

inline double qMin(double l, float r) {
    return qMin(l, static_cast<double>(r));
}

inline double qMin(float l, double r) {
    return qMin(static_cast<double>(l), r);
}

inline double qMax(double l, float r) {
    return qMax(l, static_cast<double>(r));
}

inline double qMax(float l, double r) {
    return qMax(static_cast<double>(l), r);
}

inline float qSign(float value) {
    return value >= 0.0f ? 1.0f : -1.0f;
}

inline double qSign(double value) {
    return value >= 0.0 ? 1.0 : -1.0;
}


#endif // QN_MATH_H
