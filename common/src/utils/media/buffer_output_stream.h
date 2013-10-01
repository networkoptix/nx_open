/**********************************************************
* 1 oct 2013
* a.kolesnikov
***********************************************************/

#ifndef BUFFER_OUTPUT_STREAM_H
#define BUFFER_OUTPUT_STREAM_H

#include <QByteArray>

#include "abstract_byte_stream_filter.h"


//!Appends incoming data to specified \a QByteArray
class BufferOutputStream
:
    public AbstractByteStreamFilter
{
public:
    BufferOutputStream( QByteArray* const buffer );

    //!Implementation of \a AbstractByteStreamFilter::processData
    virtual void processData( const QnByteArrayConstRef& data ) override;
    //!Implementation of \a AbstractByteStreamFilter::flush
    virtual void flush() override;

private:
    QByteArray* const m_buffer;
};

#endif  //BUFFER_OUTPUT_STREAM_H
