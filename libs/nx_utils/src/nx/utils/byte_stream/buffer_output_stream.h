#pragma once

#include <QtCore/QByteArray>

#include "abstract_byte_stream_filter.h"

namespace nx {
namespace utils {
namespace bstream {

/**
 * Saves all incoming data to internal buffer.
 */
class NX_UTILS_API BufferOutputStream:
    public AbstractByteStreamFilter
{
public:
    BufferOutputStream();
        
    virtual bool processData(const QnByteArrayConstRef& data) override;

    QByteArray buffer() const;

private:
    QByteArray m_buffer;
};

} // namespace bstream
} // namespace utils
} // namespace nx
