/**********************************************************
* May 12, 2016
* a.kolesnikov
***********************************************************/

#include "buffer_output_stream.h"


BufferOutputStream::BufferOutputStream()
:
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
