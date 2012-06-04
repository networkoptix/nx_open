#ifndef QN_MATH_H
#define QN_MATH_H

#include <cmath>

#include <QtCore/QtGlobal>
#include <QtCore/qnumeric.h>

#include "fuzzy.h"
#include "float.h"


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
inline static quint64 htonll(quint64 x)
{
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
inline static quint64 ntohll(quint64 x) { return htonll(x); }

#else
inline static quint64 htonll(quint64 x) { return x;}
inline static quint64 ntohll(quint64 x)  { return x;}
#endif


/**
 * \param value                         Value to round up.
 * \param step                          Rounding step, must be power of 2.
 * \returns                             Rounded value.
 */
inline unsigned int qPower2Ceil(unsigned int value, int step) {
    return ((value - 1) & ~(step - 1)) + step;
}

inline quint64 qPower2Ceil(quint64 value, int step) {
    return ((value - 1) & ~(step - 1)) + step;
}

/**
 * \param value                         Value to round down.
 * \param step                          Rounding step, must be power of 2.
 * \returns                             Rounded value.
 */
inline unsigned int qPower2Floor(unsigned int value, int step) {
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

/**
 * \param value                         Value to round up.
 * \param step                          Rounding step, must be positive.
 * \returns                             Rounded value.
 */
template<class T>
T qCeil(T value, T step) {
    value = value + step - 1;
    return value - qMod(value, step);
}

template<class T>
T qFloor(T value, T step) {
    return qCeil(value - step + 1, step);
}

template<class T>
T qRound(T value, T step) {
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
