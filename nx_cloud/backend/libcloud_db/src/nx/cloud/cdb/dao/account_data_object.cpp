#include "account_data_object.h"

#include <functional>

#include <nx/utils/std/cpp14.h>

#include "rdb/rdb_account_data_object.h"

namespace nx {
namespace cdb {
namespace dao {

AccountDataObjectFactory::AccountDataObjectFactory():
    base_type(std::bind(&AccountDataObjectFactory::defaultFactoryFunction, this))
{
}

AccountDataObjectFactory& AccountDataObjectFactory::instance()
{
    static AccountDataObjectFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractAccountDataObject> AccountDataObjectFactory::defaultFactoryFunction()
{
    return std::make_unique<rdb::AccountDataObject>();
}

} // namespace dao
} // namespace cdb
} // namespace nx
