#include "fuzzy.h"

#include <cmath>

#include <nx/utils/log/assert.h>

#if QT_VERSION == 0x050201
static inline quint32 f2i(float f)
{
    quint32 i;
    memcpy(&i, &f, sizeof(f));
    return i;
}

quint32 qFloatDistance(float a, float b)
{
    static const quint32 smallestPositiveFloatAsBits = 0x00000001;  // denormalized, (SMALLEST), (1.4E-45)
    /* Assumes:
       * IEE754 format.
       * Integers and floats have the same endian
    */
    Q_STATIC_ASSERT(sizeof(quint32) == sizeof(float));
    NX_ASSERT(qIsFinite(a) && qIsFinite(b));
    if (a == b)
        return 0;
    if ((a < 0) != (b < 0))
    {
// if they have different signs
        if (a < 0)
            a = -a;
        else /*if (b < 0)*/
            b = -b;
        return qFloatDistance(0.0F, a) + qFloatDistance(0.0F, b);
    }
    if (a < 0)
    {
        a = -a;
        b = -b;
    }
    // at this point a and b should not be negative

    // 0 is special
    if (!a)
        return f2i(b) - smallestPositiveFloatAsBits + 1;
    if (!b)
        return f2i(a) - smallestPositiveFloatAsBits + 1;

    // finally do the common integer subtraction
    return a > b ? f2i(a) - f2i(b) : f2i(b) - f2i(a);
}

static inline quint64 d2i(double d)
{
    quint64 i;
    memcpy(&i, &d, sizeof(d));
    return i;
}

quint64 qFloatDistance(double a, double b)
{
    static const quint64 smallestPositiveFloatAsBits = 0x1;  // denormalized, (SMALLEST)
    /* Assumes:
       * IEE754 format double precision
       * Integers and floats have the same endian
    */
    Q_STATIC_ASSERT(sizeof(quint64) == sizeof(double));
    NX_ASSERT(qIsFinite(a) && qIsFinite(b));
    if (a == b)
        return 0;
    if ((a < 0) != (b < 0))
    {
// if they have different signs
        if (a < 0)
            a = -a;
        else /*if (b < 0)*/
            b = -b;
        return qFloatDistance(0.0, a) + qFloatDistance(0.0, b);
    }
    if (a < 0)
    {
        a = -a;
        b = -b;
    }
    // at this point a and b should not be negative

    // 0 is special
    if (!a)
        return d2i(b) - smallestPositiveFloatAsBits + 1;
    if (!b)
        return d2i(a) - smallestPositiveFloatAsBits + 1;

    // finally do the common integer subtraction
    return a > b ? d2i(a) - d2i(b) : d2i(b) - d2i(a);
}
#endif

float qFuzzyFloor(float value)
{
    float result = std::floor(value);

    if (qFuzzyIsNull(value - result - 1.0f))
    {
        return result + 1.0f;
    }
    else
    {
        return result;
    }
}

double qFuzzyFloor(double value)
{
    double result = std::floor(value);

    if (qFuzzyIsNull(value - result - 1.0))
    {
        return result + 1.0;
    }
    else
    {
        return result;
    }
}

float qFuzzyCeil(float value)
{
    float result = std::ceil(value);

    if (qFuzzyIsNull(result - value - 1.0f))
    {
        return result - 1.0f;
    }
    else
    {
        return result;
    }
}

double qFuzzyCeil(double value)
{
    double result = std::ceil(value);

    if (qFuzzyIsNull(result - value - 1.0))
    {
        return result - 1.0;
    }
    else
    {
        return result;
    }
}

bool qFuzzyContains(const QRectF &rect, const QPointF &point)
{
    return
        qFuzzyBetween(rect.left(), point.x(), rect.right()) &&
        qFuzzyBetween(rect.top(), point.y(), rect.bottom());
}


