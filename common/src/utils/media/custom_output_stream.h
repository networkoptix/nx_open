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
    virtual bool processData( const QnByteArrayConstRef& data )
    {
        //TODO #ak support functor with return value
        m_func( data );
        return true;
    }

    //!Implementation of \a AbstractByteStreamFilter::flush
    virtual size_t flush() { return 0; }

private:
    Func m_func;
};

template<class Func>
    std::shared_ptr<CustomOutputStream<Func> > makeCustomOutputStream( Func func )
{
    return std::make_shared<typename CustomOutputStream<Func> >( std::move(func) );
}


//!Calls specified \a func and passes all input data to the next filter
template<class Func>
class FilterWithFunc
:
    public AbstractByteStreamFilter
{
public:
    FilterWithFunc( const Func& func )
    :
        m_func( func )
    {
    }

    //!Implementation of \a AbstractByteStreamFilter::processData
    virtual bool processData( const QnByteArrayConstRef& data )
    {
        m_func();
        return nextFilter()->processData( data );
    }

    //!Implementation of \a AbstractByteStreamFilter::flush
    virtual size_t flush() { return nextFilter()->flush(); }

private:
    Func m_func;
};

template<class Func>
    std::shared_ptr<FilterWithFunc<Func> > makeFilterWithFunc( Func func )
{
    return std::make_shared<typename FilterWithFunc<Func> >( std::move(func) );
}

#endif  //CUSTOM_OUTPUT_STREAM_H
