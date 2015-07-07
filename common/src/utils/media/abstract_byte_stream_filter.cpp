/**********************************************************
* 26 may 2015
* a.kolesnikov
***********************************************************/

#include "abstract_byte_stream_filter.h"


namespace nx_bsf
{
    //!Returns last filter of the filter chain with first element \a beginPtr
    AbstractByteStreamFilterPtr last( const AbstractByteStreamFilterPtr& beginPtr )
    {
        //using AbstractByteStreamFilterPtr* to avoid reference counter modifications
        const AbstractByteStreamFilterPtr* curElem = &beginPtr;
        while( (*curElem)->nextFilter() )
            curElem = &((*curElem)->nextFilter());
        return *curElem;
    }

    //!Inserts element \a what at position pointed to by \a _where (\a _where will be placed after \a what)
    /*!
        \note if \a _where could not be found in the chain, assert will trigger
        \return beginning of the new filter chain (it can change if \a _where equals to \a beginPtr)
    */
    AbstractByteStreamFilterPtr insert(
        const AbstractByteStreamFilterPtr& beginPtr,
        const AbstractByteStreamFilterPtr& _where,
        AbstractByteStreamFilterPtr what )
    {
        if( _where == beginPtr )
        {
            what->setNextFilter( beginPtr );
            return what;
        }

        const AbstractByteStreamFilterPtr* beforeWhere = &beginPtr;
        for( ;; )
        {
            if( (*beforeWhere)->nextFilter() == _where )
                break;
            beforeWhere = &((*beforeWhere)->nextFilter());
        }
        what->setNextFilter( _where );
        (*beforeWhere)->setNextFilter( what );
        return beginPtr;
    }
}
