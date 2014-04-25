/**********************************************************
* 1 oct 2013
* a.kolesnikov
***********************************************************/

#ifndef CUSTOM_OUTPUT_STREAM_H
#define CUSTOM_OUTPUT_STREAM_H

#include "abstract_byte_stream_filter.h"


//!Passes input data to specified function
template<class Func>
class CustomOutputStream
:
    public AbstractByteStreamFilter
{
public:
    CustomOutputStream( const Func& func )
    :
        m_func( func )
    {
    }

    //!Implementation of \a AbstractByteStreamFilter::processData
    virtual void processData( const QnByteArrayConstRef& data )
    {
        m_func( data );
    }

    //!Implementation of \a AbstractByteStreamFilter::flush
    virtual size_t flush() { return 0; }

private:
    Func m_func;
};

#endif  //CUSTOM_OUTPUT_STREAM_H
