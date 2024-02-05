// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>

namespace nx::utils::bstream::gzip {

/**
 * Deflates gzip-compressed stream. Suitable for decoding gzip http content encoding.
 */
class NX_UTILS_API Uncompressor: public AbstractByteStreamFilter
{
public:
    enum class Method
    {
        gzip,
        deflate
    };
public:
    Uncompressor(
        const std::shared_ptr<AbstractByteStreamFilter>& nextFilter = nullptr,
        Method method = Method::gzip);
    virtual ~Uncompressor() override;

    virtual bool processData(const ConstBufferRefType& data) override;
    virtual size_t flush() override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace nx::utils::bstream::gzip
