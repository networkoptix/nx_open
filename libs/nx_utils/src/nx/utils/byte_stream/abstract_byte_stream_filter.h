// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/buffer.h>

namespace nx::utils::bstream {

/**
 * Interface for class doing something with byte stream.
 * Filter which passes result stream to nextFilter
 * NOTE: Implementation is allowed to cache some data, but it SHOULD always minimize such caching.
 */
class NX_UTILS_API AbstractByteStreamFilter
{
public:
    AbstractByteStreamFilter(
        const std::shared_ptr<AbstractByteStreamFilter>& nextFilter = nullptr);

    virtual ~AbstractByteStreamFilter() = default;

    /**
     * @return false in case of error.
     */
    virtual bool processData(const ConstBufferRefType& data) = 0;

    /**
     * Implementation SHOULD process any cached data.
     * This method is usually signals end of source data.
     * @return > 0, if some data has been flushed, 0 if no data to flush
     */
    virtual size_t flush();

    /**
     * This method is virtual to allow it to become thread-safe in successor.
     */
    virtual void setNextFilter(const std::shared_ptr<AbstractByteStreamFilter>& nextFilter);

    virtual const std::shared_ptr<AbstractByteStreamFilter>& nextFilter() const;

protected:
    std::shared_ptr<AbstractByteStreamFilter> m_nextFilter;
};

using AbstractByteStreamFilterPtr = std::shared_ptr<AbstractByteStreamFilter>;

/**
 * @return The last filter of the filter chain with first element beginPtr.
 */
NX_UTILS_API AbstractByteStreamFilterPtr last(
    const AbstractByteStreamFilterPtr& beginPtr);

/**
 * Inserts element what at position pointed to by _where (_where will be placed after what).
 * NOTE: if _where could not be found in the chain, NX_ASSERT will trigger
 * @return beginning of the new filter chain (it can change if _where equals to beginPtr).
 */
NX_UTILS_API AbstractByteStreamFilterPtr insert(
    const AbstractByteStreamFilterPtr& beginPtr,
    const AbstractByteStreamFilterPtr& _where,
    AbstractByteStreamFilterPtr what);

} // namespace nx::utils::bstream
