#include "data_object_factory.h"

#include "rdb/rdb_data_object.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {

static DataObjectFactory::FactoryFunc customFactoryFunc;

std::unique_ptr<AbstractDataObject> DataObjectFactory::create()
{
    if (customFactoryFunc)
        return customFactoryFunc();
    return std::make_unique<rdb::DataObject>();
}

void DataObjectFactory::setFactoryFunc(
    DataObjectFactory::FactoryFunc newFunc)
{
    customFactoryFunc = std::move(newFunc);
}

} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
