#pragma

#include "buffer_output_stream.h"

namespace nx {
namespace utils {
namespace bsf {

BufferOutputStream::BufferOutputStream():
    AbstractByteStreamFilter(std::shared_ptr<AbstractByteStreamFilter>())
{
}
        
bool BufferOutputStream::processData(const QnByteArrayConstRef& data)
{
    m_buffer += data.toByteArrayWithRawData();
    return true;
}

QByteArray BufferOutputStream::buffer() const
{
    return m_buffer;
}

} // namespace bsf
} // namespace utils
} // namespace nx
