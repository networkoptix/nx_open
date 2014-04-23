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
    Implementation is allowed to cache some data, but it SHOULD always minimize such caching
*/
class AbstractByteStreamFilter
{
public:
    virtual ~AbstractByteStreamFilter() {}

    virtual void processData( const QnByteArrayConstRef& data ) = 0;
    //!Implementation SHOULD process any cached data. This method is usually signals end of source data
    /*!
        \return > 0, if some data has been flushed, 0 if no data to flush
    */
    virtual size_t flush() = 0;
};

/*!
    Filter which passes result stream to \a nextFilter
*/
class AbstractByteStreamConverter
:
    public AbstractByteStreamFilter
{
public:
    AbstractByteStreamConverter( const std::shared_ptr<AbstractByteStreamFilter>& nextFilter = std::shared_ptr<AbstractByteStreamFilter>() )
    :
        m_nextFilter( nextFilter )
    {
    }

    virtual ~AbstractByteStreamConverter() {}

    /*!
        This method is virtual to allow it to become thread-safe in successor
    */
    virtual void setNextFilter( const std::shared_ptr<AbstractByteStreamFilter>& nextFilter )
    {
        m_nextFilter = nextFilter;
    }

protected:
    std::shared_ptr<AbstractByteStreamFilter> m_nextFilter;
};

#endif  //ABSTRACT_BYTE_STREAM_FILTER_H

