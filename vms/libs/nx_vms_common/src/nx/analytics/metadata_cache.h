// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <boost/container/flat_map.hpp>

#include <analytics/common/object_metadata.h>
#include <nx/utils/thread/mutex.h>

#include "private/max_duration_counter.h"

namespace nx::analytics {

using namespace std::chrono;

/**
 * Quite straitforward cache. Items stored in flat map as it is supposed that them are arrived
 *  in the correct order mostly.
 *
 * @tparam T for metadata type to be stored and return on getMetadata request.
 */
template<typename T>
class MetadataCache
{
public:
    MetadataCache(const MetadataType metadataType, const milliseconds& cacheDuration = 10s);
    ~MetadataCache() = default;

    void insertMetadata(const QnConstAbstractCompressedMetadataPtr& metadata);
    void setCacheSize(const milliseconds& cacheSize);
    std::vector<T> getMetadata(const milliseconds& timestamp);

    static T uncompress(const QnConstAbstractCompressedMetadataPtr& metadata);
    static qint64 timestampUs(const T& metadata);
    static qint64 durationUs(const T& metadata);

private:
    void removeOldestMetadataItemUnsafe(const milliseconds& rightBorder);

private:
    mutable nx::Mutex m_mutex;
    milliseconds m_cacheDuration;
    MetadataType m_metadataType;
    const std::unique_ptr<MaxDurationCounter> m_maxDurationCounter;

    boost::container::flat_multimap<milliseconds, T> m_orderedMetadata;
};

} // namespace nx::analytics
