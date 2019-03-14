#include "speed_range.h"
#include <nx/utils/math/fuzzy.h>


QnSpeedRange::QnSpeedRange(qreal forward, qreal backward):
    forward(forward),
    backward(backward)
{
}

bool QnSpeedRange::fuzzyEquals(const QnSpeedRange& other) const
{
    return qFuzzyEquals(forward, other.forward) && qFuzzyEquals(backward, other.backward);
}

QnSpeedRange QnSpeedRange::expandedTo(QnSpeedRange& other) const
{
    return QnSpeedRange(qMax(forward, other.forward), qMax(backward, other.backward));
}

QnSpeedRange& QnSpeedRange::expandTo(QnSpeedRange& other)
{
    forward = qMax(forward, other.forward);
    backward = qMax(backward, other.backward);
    return *this;
}

QnSpeedRange QnSpeedRange::limitedBy(QnSpeedRange& other) const
{
    return QnSpeedRange(qMin(forward, other.forward), qMin(backward, other.backward));
}

QnSpeedRange& QnSpeedRange::limitBy(QnSpeedRange& other)
{
    forward = qMin(forward, other.forward);
    backward = qMin(backward, other.backward);
    return *this;
}

qreal QnSpeedRange::boundSpeed(qreal speed) const
{
    return qBound(-backward, speed, forward);
}
