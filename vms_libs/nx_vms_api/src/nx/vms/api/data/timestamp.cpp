#include "timestamp.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

bool Timestamp::operator<(const Timestamp& right) const
{
    return sequence != right.sequence
        ? sequence < right.sequence
        : ticks < right.ticks;
}

bool Timestamp::operator<=(const Timestamp& right) const
{
    return sequence != right.sequence
        ? sequence < right.sequence
        : ticks <= right.ticks;
}

bool Timestamp::operator==(const Timestamp& right) const
{
    return sequence == right.sequence && ticks == right.ticks;
}

bool Timestamp::operator>(quint64 right) const
{
    return sequence > 0
        ? true
        : ticks > right;
}

Timestamp& Timestamp::operator-=(qint64 right)
{
    if (right < 0)
        return operator+=(-right);

    const auto ticksBak = ticks;
    ticks -= right;
    if (ticks > ticksBak) //< Overflow?
        --sequence;
    return *this;
}

Timestamp& Timestamp::operator+=(qint64 right)
{
    if (right < 0)
        return operator-=(-right);

    const auto ticksBak = ticks;
    ticks += right;
    if (ticks < ticksBak) //< Overflow?
        ++sequence;
    return *this;
}

Timestamp& Timestamp::operator++() // ++t
{
    *this += 1;
    return *this;
}

Timestamp Timestamp::operator++(int) // t++
{
    const auto result = *this;
    *this += 1;
    return result;
}

Timestamp Timestamp::operator+(quint64 right) const
{
    Timestamp result(*this);
    result += right;
    return result;
}

QString Timestamp::toString() const
{
    return lit("(%1, %2)").arg(sequence).arg(ticks);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    Timestamp,
    (json)(ubjson)(xml)(csv_record),
    Timestamp_Fields,
    (optional, false))

} // namespace api
} // namespace vms
} // namespace nx
