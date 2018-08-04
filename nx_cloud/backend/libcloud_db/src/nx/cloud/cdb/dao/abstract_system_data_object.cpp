#include "abstract_system_data_object.h"

#include <nx/utils/std/cpp14.h>

#include "rdb/system_data_object.h"

namespace nx {
namespace cdb {
namespace dao {

static SystemDataObjectFactory::CustomFactoryFunc customFactoryFunc;

std::unique_ptr<AbstractSystemDataObject> SystemDataObjectFactory::create(
    const conf::Settings& settings)
{
    if (customFactoryFunc)
        return customFactoryFunc(settings);
    return std::make_unique<rdb::SystemDataObject>(settings);
}

SystemDataObjectFactory::CustomFactoryFunc SystemDataObjectFactory::setCustomFactoryFunc(
    CustomFactoryFunc func)
{
    customFactoryFunc.swap(func);
    return func;
}

} // namespace dao
} // namespace cdb
} // namespace nx
