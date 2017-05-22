#pragma once

#include "abstract_byte_stream_filter.h"

namespace nx {
namespace utils {
namespace bstream {

/**
 * Decodes stream of following blocks:
 * - 4 bytes. size in network byte order.
 * - n bytes of data.
 * Next filter receives one block per SizedDataDecodingFilter::processData call.
 */
class NX_UTILS_API SizedDataDecodingFilter:
    public AbstractByteStreamFilter
{
public:
    virtual bool processData(const QnByteArrayConstRef& data) override;
};

} // namespace bstream
} // namespace utils
} // namespace nx
