/**********************************************************
* 1 oct 2013
* a.kolesnikov
***********************************************************/

#include "buffer_output_stream.h"


BufferOutputStream::BufferOutputStream( QByteArray* const buffer )
:
    m_buffer( buffer )
{
}

//!Implementation of \a AbstractByteStreamFilter::processData
void BufferOutputStream::processData( const QnByteArrayConstRef& data )
{
    m_buffer->append( data.constData(), data.size() );
}

//!Implementation of \a AbstractByteStreamFilter::flush
void BufferOutputStream::flush()
{
}
