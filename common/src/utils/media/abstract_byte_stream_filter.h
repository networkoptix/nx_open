/**********************************************************
* 1 oct 2013
* a.kolesnikov
***********************************************************/

#ifndef ABSTRACT_BYTE_STREAM_FILTER_H
#define ABSTRACT_BYTE_STREAM_FILTER_H

#include <memory>

#include "../network/http/qnbytearrayref.h"


//!Interface for class doing something with byte stream
/*!
    Filter which passes result stream to \a nextFilter
    \note Implementation is allowed to cache some data, but it SHOULD always minimize such caching
*/
class AbstractByteStreamFilter
{
public:
    AbstractByteStreamFilter( const std::shared_ptr<AbstractByteStreamFilter>& nextFilter = std::shared_ptr<AbstractByteStreamFilter>() )
    :
        m_nextFilter( nextFilter )
    {
    }

    virtual ~AbstractByteStreamFilter() {}

    /*!
        \return \a false in case of error
    */
    virtual bool processData( const QnByteArrayConstRef& data ) = 0;
    //!Implementation SHOULD process any cached data. This method is usually signals end of source data
    /*!
        \return > 0, if some data has been flushed, 0 if no data to flush
    */
    virtual size_t flush()
    {
        if( !m_nextFilter )
            return 0;
        return m_nextFilter->flush();
    }

    /*!
        This method is virtual to allow it to become thread-safe in successor
    */
    virtual void setNextFilter( const std::shared_ptr<AbstractByteStreamFilter>& nextFilter )
    {
        m_nextFilter = nextFilter;
    }

    virtual const std::shared_ptr<AbstractByteStreamFilter>& nextFilter() const
    {
        return m_nextFilter;
    }

protected:
    std::shared_ptr<AbstractByteStreamFilter> m_nextFilter;
};

typedef std::shared_ptr<AbstractByteStreamFilter> AbstractByteStreamFilterPtr;


namespace nx_bsf
{
    //!Returns last filter of the filter chain with first element \a beginPtr
    AbstractByteStreamFilterPtr last( const AbstractByteStreamFilterPtr& beginPtr );
    //!Inserts element \a what at position pointed to by \a _where (\a _where will be placed after \a what)
    /*!
        \note if \a _where could not be found in the chain, assert will trigger
        \return beginning of the new filter chain (it can change if \a _where equals to \a beginPtr)
    */
    AbstractByteStreamFilterPtr insert(
        const AbstractByteStreamFilterPtr& beginPtr,
        const AbstractByteStreamFilterPtr& _where,
        AbstractByteStreamFilterPtr what );
}

#endif  //ABSTRACT_BYTE_STREAM_FILTER_H
