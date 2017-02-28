#include "speed_range.h"
#include <nx/utils/math/fuzzy.h>


QnSpeedRange::QnSpeedRange(qreal forward, qreal reverse):
    forward(forward),
    reverse(reverse)
{
}

bool QnSpeedRange::fuzzyEquals(const QnSpeedRange& other) const
{
    return qFuzzyEquals(forward, other.forward) && qFuzzyEquals(reverse, other.reverse);
}

QnSpeedRange QnSpeedRange::expandedTo(QnSpeedRange& other) const
{
    return QnSpeedRange(qMax(forward, other.forward), qMax(reverse, other.reverse));
}

QnSpeedRange& QnSpeedRange::expandTo(QnSpeedRange& other)
{
    forward = qMax(forward, other.forward);
    reverse = qMax(reverse, other.reverse);
    return *this;
}

QnSpeedRange QnSpeedRange::boundedTo(QnSpeedRange& other) const
{
    return QnSpeedRange(qMin(forward, other.forward), qMin(reverse, other.reverse));
}

QnSpeedRange& QnSpeedRange::boundTo(QnSpeedRange& other)
{
    forward = qMin(forward, other.forward);
    reverse = qMin(reverse, other.reverse);
    return *this;
}
