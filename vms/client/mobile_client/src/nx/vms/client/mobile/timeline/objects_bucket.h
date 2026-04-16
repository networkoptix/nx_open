// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include "abstract_object_data.h"

namespace nx::vms::client::mobile {
namespace timeline {

/**
 * A temporal range in the timeline, possibly containing a single or multiple object data,
 * with its state. The duration is not stored as it's constant for all buckets.
 */
struct ObjectBucket
{
    enum State
    {
        Initial,
        Empty,
        Ready
    };
    Q_ENUM(State)

    qint64 startTimeMs{};
    State state = State::Initial;
    std::optional<MultiObjectData> data;
    bool isLoading = false;

    MultiObjectData getData() const;

    Q_GADGET
    Q_PROPERTY(qint64 startTimeMs MEMBER startTimeMs CONSTANT)
    Q_PROPERTY(MultiObjectData data READ getData)
    Q_PROPERTY(State state MEMBER state)
    Q_PROPERTY(bool isLoading MEMBER isLoading)
};

} // namespace timeline
} // namespace nx::vms::client::mobile
