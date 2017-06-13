/**********************************************************
* Sep 14, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_DATA_VIEW_H
#define NX_CDB_DATA_VIEW_H

#include <functional>

#include "data_event.h"


namespace nx {
namespace cdb {


template<typename DataType>
class DataIterator
{
public:
    //!\a false means end-of-data
    bool next();
    //!Moves data out of iterator
    DataType take();
    //!Returns copy of internal data
    DataType get() const;
};

template<typename DataType>
class DataView
{
public:
    typedef DataIterator<DataType> Iterator;
    typedef DataChangeEvent<DataType> Event;

    DataView(DataView&&);
    DataView& operator=(DataView&&);

    //!Returns iterator to the first record in the data view
    /*!
        \note Creation of this iterator does not lock data set
        \note Data can be changed at any moment. Only current record is copied to the iterator
    */
    Iterator begin();
    /*!
        \note \a insert event means record qualified for the view. Record could in fact exist prior.
        Same applies for \a delete event
    */
    void setDataEventListener(std::function<void(Event)> eventReceiver);
};

}   //cdb
}   //nx

#endif  //NX_CDB_DATA_VIEW_H
