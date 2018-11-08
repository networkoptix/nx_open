#pragma once

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>

namespace nx {
namespace utils {
namespace bstream {
namespace gzip {

/**
 * Deflates gzip-compressed stream. Suitable for decoding gzip http content encoding.
 */
class NX_UTILS_API Uncompressor: public AbstractByteStreamFilter
{
public:
    Uncompressor(const std::shared_ptr<AbstractByteStreamFilter>& nextFilter = nullptr);
    virtual ~Uncompressor() override;

    virtual bool processData(const QnByteArrayConstRef& data) override;
    virtual size_t flush() override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace gzip
} // namespace bstream
} // namespace utils
} // namespace nx
