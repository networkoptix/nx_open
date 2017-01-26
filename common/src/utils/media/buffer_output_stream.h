/**********************************************************
* May 12, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <QtCore/QByteArray>

#include "abstract_byte_stream_filter.h"


/** Saves all incoming data to internal buffer */
class BufferOutputStream
:
    public AbstractByteStreamFilter
{
public:
    BufferOutputStream();
        
    virtual bool processData(const QnByteArrayConstRef& data) override;

    QByteArray buffer() const;

private:
    QByteArray m_buffer;
};
