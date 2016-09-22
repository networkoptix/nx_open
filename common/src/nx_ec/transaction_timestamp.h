#pragma once

#include <cstdint>

#include <nx/fusion/model_functions_fwd.h>

namespace ec2 {

class Timestamp
{
public:
    /**
     * This is some sequence introduced to give transactions generated after
     * unbinding from cloud / binding to cloud higher priority.
     */
    std::uint64_t sequence;
    /** This is a regular transaction timestamp. Close to millis since epoch. */
    std::uint64_t ticks;

    Timestamp():
        sequence(0),
        ticks(0)
    {
    }

    Timestamp(std::uint64_t sequence, std::uint64_t ticks):
        sequence(sequence),
        ticks(ticks)
    {
    }

    bool operator<(const Timestamp& right) const
    {
        if (sequence != right.sequence)
            return sequence < right.sequence;
        return ticks < right.ticks;
    }

    bool operator<=(const Timestamp& right) const
    {
        if (sequence != right.sequence)
            return sequence < right.sequence;
        return ticks <= right.ticks;
    }

    bool operator==(const Timestamp& right) const
    {
        return sequence == right.sequence && ticks == right.ticks;
    }

    bool operator>(const Timestamp& right) const
    {
        return right < (*this);
    }

    bool operator>=(const Timestamp& right) const
    {
        return right <= (*this);
    }

    bool operator>(std::uint64_t right) const
    {
        return (sequence > 0) ? true : (ticks > right);
    }

    Timestamp& operator-=(std::uint64_t right)
    {
        const auto ticksBak = ticks;
        ticks -= right;
        if (ticks > ticksBak)   //< Overflow?
            --sequence;
        return *this;
    }

    Timestamp& operator+=(std::uint64_t right)
    {
        const auto ticksBak = ticks;
        ticks += right;
        if (ticks < ticksBak)   //< Overflow?
            ++sequence;
        return *this;
    }

    Timestamp& operator++() // ++t
    {
        *this += 1;
        return *this;
    }

    Timestamp operator++(int) // t++
    {
        const auto result = *this;
        *this += 1;
        return result;
    }

    static Timestamp fromInteger(std::uint64_t value)
    {
        return Timestamp(value);
    }

    static Timestamp fromInteger(std::int64_t value)
    {
        return Timestamp(value);
    }

    static Timestamp fromInteger(int value)
    {
        return Timestamp(value);
    }

private:
    Timestamp(std::uint64_t value):
        sequence(0),
        ticks(value)
    {
    }

    Timestamp(std::int64_t value):
        sequence(0),
        ticks(static_cast<std::uint64_t>(value))
    {
    }

    Timestamp(int value):
        sequence(0),
        ticks(static_cast<std::uint64_t>(value))
    {
    }

    Timestamp& operator=(int value)
    {
        sequence = 0;
        ticks = static_cast<std::uint64_t>(value);
        return *this;
    }

    Timestamp& operator=(std::uint64_t value)
    {
        sequence = 0;
        ticks = value;
        return *this;
    }

    Timestamp& operator=(std::int64_t value)
    {
        sequence = 0;
        ticks = static_cast<std::uint64_t>(value);
        return *this;
    }
};

Timestamp operator+(const Timestamp& left, std::uint64_t right);

QString toString(const Timestamp& val);

#define Timestamp_Fields (sequence)(ticks)

} // namespace ec2

QN_FUSION_DECLARE_FUNCTIONS(::ec2::Timestamp, (json)(ubjson)(xml)(csv_record))
