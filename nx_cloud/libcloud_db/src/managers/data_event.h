/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_data_event_h
#define cloud_db_data_event_h

#include <stdint.h>


namespace nx {
namespace cdb {

typedef uint64_t TransactionSequence;
static const TransactionSequence INVALID_TRANSACTION = (uint64_t)-1;

//!In case of \a type == \a etInsert or \a etUpdate object can be cast to corresponding \a DataInsertUpdateEvent
class DataChangeEvent
{
public:
    enum EventType
    {
        etInsert,
        etUpdate,
        etDelete
    };
    
    EventType type;
    //!Transaction sequence. Transaction sequence is unique for ALL modifications to DB
    TransactionSequence tranSeq;
    //!ID of object changed
    QnUuid id;
};

template<class DataType>
class DataInsertUpdateEvent
:
    public DataChangeEvent
{
public:
    DataType data;
};

class DataDeleteEvent
:
    public DataChangeEvent
{
public:
};

}   //cdb
}   //nx

#endif
