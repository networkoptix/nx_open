#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <type_traits>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::clusterdb::engine {

class NX_DATA_SYNC_ENGINE_API Timestamp
{
public:
    /**
     * This is some sequence introduced to give transactions generated after
     * unbinding from cloud / binding to cloud higher priority.
     */
    quint64 sequence = 0;

    /** This is a regular transaction timestamp. Close to millis since epoch. */
    quint64 ticks = 0;

    Timestamp() = default;

    Timestamp(quint64 sequence, quint64 ticks):
        sequence(sequence),
        ticks(ticks)
    {
    }

    Timestamp(quint64 sequence, std::chrono::milliseconds ticks):
        sequence(sequence),
        ticks(ticks.count())
    {
    }

    bool operator==(const Timestamp& right) const;
    bool operator!=(const Timestamp& right) const { return !(*this == right); }

    bool operator<(const Timestamp& right) const;
    bool operator<=(const Timestamp& right) const;
    bool operator>(const Timestamp& right) const { return right < (*this); }
    bool operator>=(const Timestamp& right) const { return right <= (*this); }

    bool operator>(std::chrono::milliseconds right) const;

    Timestamp& operator-=(std::chrono::milliseconds right);
    Timestamp& operator+=(std::chrono::milliseconds right);

    Timestamp& operator++(); // ++t
    Timestamp operator++(int); // t++

    Timestamp operator+(std::chrono::milliseconds right) const;

    std::string toString() const;
};

#define Timestamp_Fields (sequence)(ticks)

QN_FUSION_DECLARE_FUNCTIONS(
    Timestamp,
    (json)(ubjson),
    NX_DATA_SYNC_ENGINE_API)

} // namespace nx::clusterdb::engine
