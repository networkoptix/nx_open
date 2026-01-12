// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <list>
#include <map>
#include <optional>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QStringList>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/format.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <recording/time_period.h>

#include "data_list_helpers.h"

namespace nx::vms::client::core {
namespace timeline {

/**
 * Cache implementation for CachingProxyBackend<DataList>.
 * Caches time period blocks with data sorted by timestamp in descending order.
 *
 * Blocks can be complete and incomplete (with data cropped by size limit).
 * For simplicity incomplete blocks are split into a large complete block (from the first complete
 * millisecond to the end time of the period) and an one-millisecond incomplete block (just the
 * potentially cropped earliest millisecond of the source block). So in the cache implementation,
 * an "incomplete block" is always 1ms long.
 *
 * The cache maintains a linked list of time period blocks. Accessed blocks are moved to the top.
 * If the maximum number of blocks (`maximumBlockCount`) or the maximum total number of data items
 * (`maximumCacheSize`) is exceeded, the least recently accessed bottom of the list is discarded.
 *
 * The cache supports expiration by time which can be optionally passed to the `add` method.
 * When a time period block is expired, it's eventually discarded. For optimization there is no
 * periodic checks and cleanups of the entire cache; instead, when an attempt to access a block is
 * made - during a cache lookup or during an addition of a new block overlapping existing blocks -
 * expiration is checked and expired blocks are discarded.
 *
 * For testing purposes it's valid to add an already expired block (passing 0s to `add`).
 *
 * If an incoming block intersects with existing blocks, it's subtracted (in terms of time periods)
 * from the existing blocks (they can be cut, split or erased), and then added as a new block.
 *
 * For simplicity, an incoming block always takes precedence over an existing block. An expirable
 * block can overwrite a block that never expires, and an incomplete block can overwrite a part of
 * a complete block.
 *
 * Empty neighboring never expiring blocks are merged together.
 * Non-empty or expirable neighboring blocks are never merged.
 *
 * This class is NOT thread-safe.
 */
template<typename DataList>
class DataListsCache
{
    using Helpers = DataListHelpers<DataList>;
    using milliseconds = std::chrono::milliseconds;
    using seconds = std::chrono::seconds;

public:
    /**
     * A cached time period with data.
     * Expirable or not.
     * Can be "incomplete" - that means it contains only one millisecond of incomplete data.
     */
    class Block
    {
    public:
        /**
         * A shared counter to track the total number of data items in all blocks.
         */
        struct DataCounter
        {
            int operator()() const { return value; }
            friend class Block;
        private:
            int value = 0;
        };

        Block(Block&&) = default;
        ~Block();

        /** Construct a complete block. */
        Block(DataCounter* counter, const QnTimePeriod& period, DataList data,
            milliseconds expirationTime /*negative to never expire*/);

        /** Construct an incomplete one-millisecond block. */
        Block(DataCounter* counter, milliseconds timestamp, DataList incompleteOneMsData,
            milliseconds expirationTime /*negative to never expire*/);

        const QnTimePeriod& period() const { return m_period; }
        const DataList& data() const { return m_data; }
        int size() const { return (int) m_data.size(); }
        bool complete() const { return m_complete; }
        bool incomplete() const { return !m_complete; }
        bool expired() const { return m_expiration.hasExpired(); }
        bool neverExpires() const { return m_expiration.isForever(); }
        milliseconds remainingTime() const { return milliseconds(m_expiration.remainingTime()); }

        QString info() const; //< For logging.

        /** Clip the block to a smaller period. `period` must be encompassed by `m_period`. */
        void clipToPeriod(const QnTimePeriod& period);

        /** Expand the block to a larger period. `period` must encompass `m_period`. */
        void expandPeriod(const QnTimePeriod& period);

    private:
        Block(DataCounter* counter, const QnTimePeriod& period, DataList data,
            bool complete, milliseconds expirationTime /*negative to never expire*/);

    private:
        DataCounter* const m_counter;
        QnTimePeriod m_period;
        DataList m_data;
        bool m_complete = false;
        QDeadlineTimer m_expiration;
    };

    using BlockList = std::list<Block>;
    using BlockIt = BlockList::iterator;
    using BlockMap = std::map<milliseconds /*endTime*/, BlockIt, std::greater<milliseconds>>;

    const BlockList& cache() const { return m_cache; }
    const BlockMap& lookup() const { return m_lookup; }

    int cacheSize() const { return m_totalCount(); }

    int maximumCacheSize() const { return m_maximumCacheSize; }
    void setMaximumCacheSize(int value);

    int maximumBlockCount() const { return m_maximumBlockCount; }
    void setMaximumBlockCount(int value);

    /**
     * Attempt to get full data from the cache. If the data for the specified period and limit
     * is present in the cache only partially, it's not returned.
     */
    std::optional<DataList> get(const QnTimePeriod& period, int limit);

    /**
     * Add data into the cache.
     */
    void add(const QnTimePeriod& period, int limit, DataList data,
        const std::optional<seconds>& expiration = std::nullopt);

    /**
     * Clear the entire cache.
     */
    void clear();

private:
    int m_maximumCacheSize = 5000;
    int m_maximumBlockCount = 1000;

    Block::DataCounter m_totalCount; //< Must precede `m_cache`, being referenced by its items.
    BlockList m_cache;
    BlockMap m_lookup;

private:
    /**
     * Erase blocks from both `m_cache` and `m_lookup`.
     */
    BlockMap::iterator eraseBlock(BlockMap::iterator it);
    BlockMap::iterator eraseBlocks(BlockMap::iterator begin, BlockMap::iterator end);
    BlockList::iterator eraseBlock(BlockList::iterator blockIt);

    /**
     * Merge specified block with the next block if it exists, is neighboring without a gap,
     * both blocks are empty, and both blocks never expire. Doesn't invalidate `it`.
     */
    void consumeNextEmptyBlock(BlockMap::iterator it);

    /**
     * Add normal (complete) block into the cache.
     */
    void addNormalBlock(QnTimePeriod period, DataList data,
        const std::optional<seconds>& expiration = std::nullopt);

    /**
     * Add incomplete one-millisecond block into the cache.
     */
    void addIncompleteBlock(milliseconds timestamp, DataList data,
        const std::optional<seconds>& expiration = std::nullopt);

    /**
     * Trim the cache to `maximumCacheSize` and `maximumBlockCount`, if needed.
     */
    void trim();

    /**
     * Move specified block to the end of the list, making it the last to remove when trimmed.
     */
    void touch(BlockIt blockIt);
};

// ------------------------------------------------------------------------------------------------
// DataListsCache<DataList>::Block implementation.

template<typename DataList>
DataListsCache<DataList>::Block::Block(DataCounter* counter,
    const QnTimePeriod& period,
    DataList data,
    milliseconds expirationTime)
    :
    Block(counter, period, std::move(data), /*complete*/ true, expirationTime)
{
}

template<typename DataList>
DataListsCache<DataList>::Block::Block(DataCounter* counter,
    milliseconds timestamp,
    DataList incompleteOneMsData,
    milliseconds expirationTime)
    :
    Block(counter, QnTimePeriod(timestamp, milliseconds(1)), std::move(incompleteOneMsData),
        /*complete*/ false, expirationTime)
{
}

template<typename DataList>
DataListsCache<DataList>::Block::Block(DataCounter* counter,
    const QnTimePeriod& period,
    DataList data,
    bool complete,
    milliseconds expirationTime)
    :
    m_counter(counter),
    m_period(period),
    m_data(std::move(data)),
    m_complete(complete),
    // QDeadlineTimer(qint64) treats negative values as unlimited duration.
    // QDeadlineTimer(std::chrono::milliseconds) treats negative values as expired duration.
    m_expiration(expirationTime.count())
{
    NX_CRITICAL(counter);
    m_counter->value += size();
}

template<typename DataList>
DataListsCache<DataList>::Block::~Block()
{
    m_counter->value -= size();
}

template<typename DataList>
QString DataListsCache<DataList>::Block::info() const
{
    if (incomplete())
        return nx::format("incomplete block [%1] (%2 items)", m_period.startTimeMs, m_data.size());

    return nx::format("block [%1, %2) (%3 items)", m_period.startTimeMs, m_period.endTimeMs(),
        m_data.size());
}

template<typename DataList>
void DataListsCache<DataList>::Block::clipToPeriod(const QnTimePeriod& period)
{
    m_period = period;
    m_counter->value -= size();
    Helpers::clip(m_data, period);
    m_counter->value += size();
}

template<typename DataList>
void DataListsCache<DataList>::Block::expandPeriod(const QnTimePeriod& period)
{
    NX_ASSERT(size() == 0);
    m_period.addPeriod(period);
}

// ------------------------------------------------------------------------------------------------
// DataListsCache<DataList> implementation.

template<typename DataList>
void DataListsCache<DataList>::setMaximumCacheSize(int value)
{
    if (m_maximumCacheSize == value || !NX_ASSERT(value > 0))
        return;

    NX_VERBOSE(this, "Changing maximum cache size from %1 to %2", m_maximumCacheSize, value);
    m_maximumCacheSize = value;
    trim();
}

template<typename DataList>
void DataListsCache<DataList>::setMaximumBlockCount(int value)
{
    if (m_maximumBlockCount == value || !NX_ASSERT(value > 0))
        return;

    NX_VERBOSE(this, "Changing maximum block count from %1 to %2", m_maximumBlockCount, value);
    m_maximumBlockCount = value;
    trim();
}

template<typename DataList>
std::optional<DataList> DataListsCache<DataList>::get(const QnTimePeriod& period, int limit)
{
    NX_VERBOSE(this, "Doing a cache lookup for [%1, %2), limit=%3",
        period.startTimeMs, period.endTimeMs(), limit);

    auto lookupFailureLogGuard = nx::utils::makeScopeGuard(
        [this]() { NX_VERBOSE(this, "Cache lookup failed: no full coverage"); });

    auto it = m_lookup.upper_bound(period.endTime());
    if (it == m_lookup.cbegin())
        return {};

    --it;
    if (it->second->expired())
    {
        NX_VERBOSE(this, "Dropping expired %1 from the cache", it->second->info());
        eraseBlock(it);
        return {};
    }

    DataList result = Helpers::slice(it->second->data(), period, limit);
    milliseconds prevStartTime = it->second->period().startTime();
    bool prevBlockIncomplete = false;
    touch(it->second);

    int count = 1;
    while ((int) result.size() < limit && period.startTime() < prevStartTime)
    {
        if (++it == m_lookup.cend())
            return {};

        if (it->second->expired())
        {
            NX_VERBOSE(this, "Dropping expired %1 from the cache", it->second->info());
            eraseBlock(it);
            return {};
        }

        // If there's a gap before the next block.
        if (prevBlockIncomplete || it->second->period().endTime() < prevStartTime)
            return {};

        Helpers::append(result,
            Helpers::slice(it->second->data(), period, limit - (int) result.size()));
        prevStartTime = it->second->period().startTime();
        prevBlockIncomplete = it->second->incomplete();
        touch(it->second);
        ++count;
    }

    // If we ended up with an incomplete block and didn't fill up the requested limit.
    if (prevBlockIncomplete && (int) result.size() < limit)
        return {};

    lookupFailureLogGuard.disarm();

    NX_VERBOSE(this, "Cache lookup successful; collected %1 items from %2 blocks",
        result.size(), count);

    return result;
}

template<typename DataList>
void DataListsCache<DataList>::add(const QnTimePeriod& period, int limit, DataList data,
    const std::optional<seconds>& expiration)
{
    const auto dataPeriod = Helpers::coveredPeriod(data);
    if (!NX_ASSERT(limit > 0) || !NX_ASSERT(data.empty() || period.contains(dataPeriod)))
        return;

    NX_VERBOSE(this, "A new time period [%1, %2) is being added (size=%3, limit=%4, expiration=%5)",
        period.startTimeMs, period.endTimeMs(), data.size(), limit, expiration);

    if (/*complete*/ ((int) data.size() < limit))
    {
        // If the data for the period is complete, treat it as a single block.
        NX_VERBOSE(this, "The data is complete; one normal block [%1, %2) will be added",
            period.startTimeMs, period.endTimeMs());
        addNormalBlock(period, std::move(data), expiration);
    }
    else
    {
        // If the data for the period is incomplete, split it into two blocks: one containing only
        // the last (the smallest in absolute value) incomplete millisecond, and one containing
        // everything else.

        const QnTimePeriod incompleteMillisecond(dataPeriod.startTime(), milliseconds(1));

        const auto completePeriod = QnTimePeriod::fromInterval(
            incompleteMillisecond.endTime(), period.endTime());

        // If there's no complete part, add just the incomplete millisecond.
        if (completePeriod.isEmpty())
        {
            NX_VERBOSE(this, "The data is incomplete; one incomplete block [%1] will be added",
                incompleteMillisecond.startTimeMs);

            addIncompleteBlock(incompleteMillisecond.startTime(), std::move(data), expiration);
        }
        // Otherwise add both.
        else
        {
            NX_VERBOSE(this, "The data is incomplete; one incomplete block [%1] and one normal "
                "block [%2, %3) will be added", incompleteMillisecond.startTimeMs,
                completePeriod.startTimeMs, completePeriod.endTimeMs());

            addIncompleteBlock(incompleteMillisecond.startTime(),
                Helpers::slice(data, incompleteMillisecond), expiration);

            Helpers::clip(data, completePeriod);
            addNormalBlock(completePeriod, std::move(data), expiration);
        }
    }

    trim();
}

template<typename DataList>
void DataListsCache<DataList>::addNormalBlock(QnTimePeriod period, DataList data,
    const std::optional<seconds>& expiration)
{
    // Find the first block in the cache that ends in or beyond the updating range (earlier in time).
    auto firstInIt = m_lookup.lower_bound(period.endTime());

    // Clip first overlapping block in the cache, if present.
    if (firstInIt != m_lookup.cbegin())
    {
        const auto it = std::prev(firstInIt);

        // Erase the previous block if it's expired.
        if (it->second->expired())
        {
            NX_VERBOSE(this, "Dropping expired %1 from the cache", it->second->info());
            eraseBlock(it);
        }
        // If the previous block is overlapped in the middle, split it in two,
        // cutting away the middle.
        else if (it->second->period().startTime() < period.startTime())
        {
            NX_ASSERT(it->second->complete());

            const auto leftPeriod = QnTimePeriod::fromInterval(
                period.endTime(), it->second->period().endTime());

            const auto rightPeriod = QnTimePeriod::fromInterval(
                it->second->period().startTime(), period.startTime());

            NX_VERBOSE(this, "The %1 is being split in two by an incoming block [%2, %3)",
                it->second->info(), period.startTimeMs, period.endTimeMs());

            // Construct and insert the new block.
            const auto newBlockIt = m_cache.emplace(std::next(it->second), &m_totalCount,
                rightPeriod, Helpers::slice(it->second->data(), rightPeriod),
                it->second->remainingTime());

            firstInIt = m_lookup.emplace_hint(firstInIt, rightPeriod.endTime(), newBlockIt);
            NX_VERBOSE(this, "First part: %1", newBlockIt->info());

            // Merge empty neighboring blocks, if all conditions are met.
            consumeNextEmptyBlock(firstInIt);

            // Clip the original block.
            it->second->clipToPeriod(leftPeriod);
            NX_VERBOSE(this, "Second part: %1", it->second->info());
        }
        // If the previous block is overlapped at the right, clip it.
        else if (it->second->period().startTime() < period.endTime())
        {
            NX_VERBOSE(this, "The %1 is being cut by an incoming block [%2, %3)",
                it->second->info(), period.startTimeMs, period.endTimeMs());

            it->second->clipToPeriod(QnTimePeriod::fromInterval(period.endTime(),
                it->second->period().endTime()));

            NX_VERBOSE(this, "Result: %1", it->second->info());
        }
    }

    // Find the first block in the cache that ends beyond the updating range (earlier in time).
    auto firstOutIt = m_lookup.lower_bound(period.startTime());

    // Clip last overlapping block in the cache, if present.
    if (firstOutIt != m_lookup.cbegin())
    {
        const auto it = std::prev(firstOutIt);

        // Erase the block if it's expired.
        if (it->second->expired())
        {
            if (firstInIt == it)
                firstInIt = firstOutIt;
            NX_VERBOSE(this, "Dropping expired %1 from the cache", it->second->info());
            eraseBlock(it);
        }
        // Clip the block if it's overlapped, and adjust `firstOutIt`.
        else if (it->second->period().startTime() < period.startTime())
        {
            NX_VERBOSE(this, "The %1 is being cut by an incoming block [%2, %3)",
                it->second->info(), period.startTimeMs, period.endTimeMs());

            const auto blockIt = it->second;
            blockIt->clipToPeriod(blockIt->period().truncated(period.startTimeMs));

            NX_VERBOSE(this, "Result: %1", blockIt->info());

            // Re-insert it in the map with the changed key.
            const bool firstInItInvalidated = firstInIt == it;
            const auto nextPosIt = m_lookup.erase(it);
            firstOutIt = m_lookup.emplace_hint(nextPosIt, blockIt->period().endTime(), blockIt);

            if (firstInItInvalidated)
                firstInIt = firstOutIt;
        }
    }

    const auto buildBlockInfoList =
        [&]() -> QString
        {
            QStringList result;
            for (auto it = firstInIt; it != firstOutIt; ++it)
                result.push_back(it->second->info());
            return nx::format("%1 blocks (%2)", result.size(), result.join(", "));
        };

    if (firstInIt != firstOutIt)
    {
        NX_VERBOSE(this, "Erasing %1 fully overlapped by an incoming block [%2, %3)",
            buildBlockInfoList(), period.startTimeMs, period.endTimeMs());
    }

    // Erase all blocks that are completely inside the updating period, if any.
    const auto nextPosIt = eraseBlocks(firstInIt, firstOutIt);

    // Construct and insert the new block.
    NX_VERBOSE(this, "Inserting block [%1, %2) (%3 items) into the cache (expirationTime=%4)",
        period.startTimeMs, period.endTimeMs(), data.size(), expiration);

    const auto blockIt = m_cache.emplace(m_cache.end(), &m_totalCount, period, std::move(data),
        expiration.value_or(seconds(-1)));

    const auto it = m_lookup.emplace_hint(nextPosIt, period.endTime(), blockIt);

    // Merge empty neighboring blocks, if all conditions are met.
    consumeNextEmptyBlock(it);
    if (it != m_lookup.begin())
        consumeNextEmptyBlock(std::prev(it));
}

template<typename DataList>
void DataListsCache<DataList>::addIncompleteBlock(milliseconds timestamp, DataList data,
    const std::optional<seconds>& expiration)
{
    const QnTimePeriod period(timestamp, milliseconds(1));
    auto firstOutIt = m_lookup.lower_bound(timestamp);

    // If a previous block that possibly contains `timestamp` exists in the cache...
    if (firstOutIt != m_lookup.begin())
    {
        const auto it = std::prev(firstOutIt);

        // Erase the previous block if it's expired or is the same period as the new block.
        if (it->second->expired())
        {
            NX_VERBOSE(this, "Dropping expired %1 from the cache", it->second->info());
            eraseBlock(it);
        }
        else if (it->second->period() == period)
        {
            NX_VERBOSE(this, "Erasing %1 fully overlapped by an incoming incomplete block [%2]",
                it->second->info(), timestamp.count());
            eraseBlock(it);
        }
        else if (it->second->period().contains(timestamp))
        {
            const auto leftPeriod = QnTimePeriod::fromInterval(
                period.endTime(), it->second->period().endTime());

            const auto rightPeriod = QnTimePeriod::fromInterval(
                it->second->period().startTime(), period.startTime());

            // Split the overlapping block in the cache in two, if needed...
            if (leftPeriod.isValid() && rightPeriod.isValid())
            {
                NX_VERBOSE(this,
                    "The %1 is being split in two by an incoming incomplete block [%2]",
                    it->second->info(), timestamp.count());

                // Construct and insert the new block.
                const auto newBlockIt = m_cache.emplace(std::next(it->second), &m_totalCount,
                    rightPeriod, Helpers::slice(it->second->data(), rightPeriod),
                    it->second->remainingTime());

                firstOutIt = m_lookup.emplace_hint(firstOutIt, rightPeriod.endTime(), newBlockIt);
                NX_VERBOSE(this, "First part: %1", newBlockIt->info());

                // Clip the original block.
                it->second->clipToPeriod(leftPeriod);
                NX_VERBOSE(this, "Second part: %1", it->second->info());
            }
            // ... or clip it at the right...
            else if (leftPeriod.isValid())
            {
                NX_VERBOSE(this, "The %1 is being cut by an incoming incomplete block [%2]",
                    it->second->info(), timestamp.count());

                it->second->clipToPeriod(QnTimePeriod::fromInterval(period.endTime(),
                    it->second->period().endTime()));

                NX_VERBOSE(this, "Result: %1", it->second->info());
            }
            // ... or clip it at the left.
            else if (NX_ASSERT(rightPeriod.isValid()))
            {
                NX_VERBOSE(this, "The %1 is being cut by an incoming incomplete block [%2]",
                    it->second->info(), timestamp.count());

                const auto blockIt = it->second;
                blockIt->clipToPeriod(blockIt->period().truncated(period.startTimeMs));

                // Re-insert it in the map with the changed key.
                const auto nextPosIt = m_lookup.erase(it);
                firstOutIt = m_lookup.emplace_hint(nextPosIt, blockIt->period().endTime(), blockIt);

                NX_VERBOSE(this, "Result: %1", blockIt->info());
            }
        }
    }

    // Construct and insert the new block.
    NX_VERBOSE(this,
        "Inserting incomplete block [%1] (%2 items) into the cache (expirationTime=%3)",
        timestamp.count(), data.size(), expiration);

    const auto blockIt = m_cache.emplace(m_cache.end(), &m_totalCount,
        timestamp, std::move(data), expiration.value_or(seconds(-1)));

    m_lookup.emplace_hint(firstOutIt, blockIt->period().endTime(), blockIt);
}

template<typename DataList>
void DataListsCache<DataList>::trim()
{
    NX_VERBOSE(this, "Current block count: %1, maximum block count: %2; "
        "current cache size: %3, maximum cache size: %4",
        m_cache.size(), m_maximumBlockCount, cacheSize(), m_maximumCacheSize);

    while ((int) m_cache.size() > m_maximumBlockCount)
    {
        NX_VERBOSE(this, "Dropping %1 from the cache (maximum block count exceeded)",
            m_cache.begin()->info());

        eraseBlock(m_cache.begin());
    }

    for (auto blockIt = m_cache.begin();
        cacheSize() > m_maximumCacheSize && NX_ASSERT(blockIt != m_cache.end()); )
    {
        const int blockSize = blockIt->size();
        if (blockSize == 0) //< Keep empty blocks.
        {
            ++blockIt;
            continue;
        }

        NX_VERBOSE(this, "Dropping %1 from the cache (maximum cache size exceeded)",
            blockIt->info());

        blockIt = eraseBlock(blockIt);
    }
}

template<typename DataList>
void DataListsCache<DataList>::touch(BlockIt blockIt)
{
    // Doesn't invalidate `blockIt`.
    NX_VERBOSE(this, "Moving %1 to the top of the list", blockIt->info());
    m_cache.splice(m_cache.end(), m_cache, blockIt, std::next(blockIt));
}

template<typename DataList>
void DataListsCache<DataList>::clear()
{
    NX_VERBOSE(this, "Clearing the cache");
    m_lookup.clear();
    m_cache.clear();
    NX_ASSERT(cacheSize() == 0);
}

template<typename DataList>
DataListsCache<DataList>::BlockList::iterator DataListsCache<DataList>::eraseBlock(
    BlockList::iterator blockIt)
{
    const auto it = m_lookup.find(blockIt->period().endTime());
    if (NX_ASSERT(it != m_lookup.end() && it->second == blockIt))
        m_lookup.erase(it);
    return m_cache.erase(blockIt);
}

template<typename DataList>
DataListsCache<DataList>::BlockMap::iterator DataListsCache<DataList>::eraseBlock(
    BlockMap::iterator it)
{
    m_cache.erase(it->second);
    return m_lookup.erase(it);
}

template<typename DataList>
DataListsCache<DataList>::BlockMap::iterator DataListsCache<DataList>::eraseBlocks(
    BlockMap::iterator begin, BlockMap::iterator end)
{
    for (auto it = begin; it != end; ++it)
        m_cache.erase(it->second);

    return m_lookup.erase(begin, end);
}

template<typename DataList>
void DataListsCache<DataList>::consumeNextEmptyBlock(BlockMap::iterator it)
{
    const auto nextIt = std::next(it);
    if (nextIt == m_lookup.end()
        || !it->second->neverExpires()
        || it->second->size() != 0
        || !nextIt->second->neverExpires()
        || nextIt->second->size() != 0
        || nextIt->second->period().endTime() != it->second->period().startTime())
    {
        return;
    }

    NX_VERBOSE(this, "Merging empty block [%1, %2) with the empty block [%3, %4)",
        nextIt->second->period().startTimeMs, nextIt->second->period().endTimeMs(),
        it->second->period().startTimeMs, it->second->period().endTimeMs());

    it->second->expandPeriod(nextIt->second->period());
    eraseBlock(nextIt);
    touch(it->second);
}

} // namespace timeline
} // namespace nx::vms::client::core
