// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <limits>
#include <optional>

#include <stddef.h>
#include <stdint.h>

#include <nx/utils/log/assert.h>

namespace nx::rtp {

template<
    typename T,
    typename std::enable_if<
        std::is_integral<T>::value
        && !std::is_same<T, bool>::value
        && std::is_unsigned<T>::value
        && sizeof(T) < sizeof(int64_t),
        T
    >::type* = nullptr
>
struct TimeLinearizer
{
    static constexpr int64_t kRange = 1 + (int64_t) std::numeric_limits<T>::max();
    static constexpr int64_t kHalfRange = kRange / 2;

    TimeLinearizer(uint64_t startValue = std::numeric_limits<int64_t>::max() / 2 + 1):
        m_highPart(startValue)
    {
        NX_ASSERT(m_highPart >= 0 && m_highPart % kRange == 0);
    }

    int64_t linearize(T sequence)
    {
        if (!m_prevSequence)
        {
            m_prevSequence = sequence;
            return m_highPart + sequence;
        }
        if (sequence < *m_prevSequence && *m_prevSequence - sequence >= kHalfRange)
        {
            m_highPart += kRange;
        }
        else if (sequence > *m_prevSequence && sequence - *m_prevSequence > kHalfRange)
        {
            m_highPart -= kRange;
        }
        *m_prevSequence = sequence;
        return m_highPart + sequence;
    }

private:
    std::optional<T> m_prevSequence;
    int64_t m_highPart = 0;
};

} // namespace nx::rtp
