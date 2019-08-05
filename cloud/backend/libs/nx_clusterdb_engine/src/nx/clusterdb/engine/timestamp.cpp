#include "timestamp.h"

#include <nx/fusion/model_functions.h>

namespace nx::clusterdb::engine {

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

bool Timestamp::operator>(std::chrono::milliseconds right) const
{
    return sequence > 0 ? true : static_cast<std::int64_t>(ticks) > right.count();
}

Timestamp& Timestamp::operator-=(std::chrono::milliseconds right)
{
    if (right < std::chrono::milliseconds::zero())
        return operator+=(-right);

    const auto ticksBak = ticks;
    ticks -= right.count();
    if (ticks > ticksBak) //< Overflow?
        --sequence;
    return *this;
}

Timestamp& Timestamp::operator+=(std::chrono::milliseconds right)
{
    if (right < std::chrono::milliseconds::zero())
        return operator-=(-right);

    const auto ticksBak = ticks;
    ticks += right.count();
    if (ticks < ticksBak) //< Overflow?
        ++sequence;
    return *this;
}

Timestamp& Timestamp::operator++() // ++t
{
    *this += std::chrono::milliseconds(1);
    return *this;
}

Timestamp Timestamp::operator++(int) // t++
{
    const auto result = *this;
    *this += std::chrono::milliseconds(1);
    return result;
}

Timestamp Timestamp::operator+(std::chrono::milliseconds right) const
{
    Timestamp result(*this);
    result += right;
    return result;
}

std::string Timestamp::toString() const
{
    return "(" + std::to_string(sequence) + ", " + std::to_string(ticks) + ")";
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    Timestamp,
    (json)(ubjson),
    Timestamp_Fields,
    (optional, false))

} // namespace nx::clusterdb::engine
