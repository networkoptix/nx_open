/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_data_event_h
#define cloud_db_data_event_h


namespace nx {
namespace cdb {

//!In case of \a type == \a etInsert or \a etUpdate object can be cast to corresponding \a DataInsertUpdateEvent
template<typename DataType>
class DataChangeEvent
{
public:
    enum EventType
    {
        etNone,
        etInsert,
        etUpdate,
        etDelete
    };

    DataChangeEvent() : type(etNone) {}

    EventType type;
    DataType data;
};

}   //cdb
}   //nx

#endif
