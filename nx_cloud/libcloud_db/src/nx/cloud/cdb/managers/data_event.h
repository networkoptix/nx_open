#pragma once

namespace nx {
namespace cdb {

/**
 * In case of type == etInsert or etUpdate object can be cast to corresponding DataInsertUpdateEvent.
 */
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

} // namespace cdb
} // namespace nx
