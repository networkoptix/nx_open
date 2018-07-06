#pragma once

#include <functional>

#include "data_event.h"

namespace nx {
namespace cdb {

template<typename DataType>
class DataIterator
{
public:
    /** False means end-of-data. */
    bool next();
    /** Moves data out of iterator. */
    DataType take();
    /**
     * @return copy of internal data.
     */
    DataType get() const;
};

template<typename DataType>
class DataView
{
public:
    using Iterator = DataIterator<DataType>;
    using Event = DataChangeEvent<DataType>;

    DataView(DataView&&);
    DataView& operator=(DataView&&);

    /**
     * @return Iterator to the first record in the data view.
     * NOTE: Creation of this iterator does not lock data set
     * NOTE: Data can be changed at any moment. Only current record is copied to the iterator
     */
    Iterator begin();
    /**
     * NOTE: insert event means record qualified for the view. Record could in fact exist prior.
     * Same applies for delete event.
     */
    void setDataEventListener(std::function<void(Event)> eventReceiver);
};

} // namespace cdb
} // namespace nx
