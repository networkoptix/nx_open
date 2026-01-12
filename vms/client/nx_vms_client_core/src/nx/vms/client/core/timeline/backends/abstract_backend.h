// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QFuture>

#include <core/resource/resource_fwd.h>
#include <recording/time_period.h>

#include "data_list_traits.h" //< To specialize for typed descendants.

namespace nx::vms::client::core {
namespace timeline {

/**
 * An abstract asynchronous data loading interface used internally by some descendants of
 * `AbstractLoaderDelegate`, used by `ObjectsLoader`.
 *
 * Backends ownership MUST be maintained via shared pointers (see `BackendPtr` below).
 * They use shared_from_this.
 *
 * Operates on API data lists sorted by timestamp in descending order (see `BookmarksBackend`,
 * `AnalyticsBackend`).
 * Can be organized in proxy chains for various optimizations (see `CachingProxyBackend`,
 * `AggregatingProxyBackend`).
 */
template<typename DataList>
class AbstractBackend: public std::enable_shared_from_this<AbstractBackend<DataList>>
{
public:
    AbstractBackend() = default;
    virtual ~AbstractBackend() = default;

    /** Returns the resource for which this backend loads data. */
    virtual QnResourcePtr resource() const = 0;

    /**
     * Asynchronously load data with timestamps within `period`, with result length
     * limited to `limit`. Result must be sorted by timestamp in descending order.
     */
    virtual QFuture<DataList> load(const QnTimePeriod& period, int limit) = 0;
};

template<typename DataList>
using BackendPtr = std::shared_ptr<AbstractBackend<DataList>>;

} // namespace timeline
} // namespace nx::vms::client::core
