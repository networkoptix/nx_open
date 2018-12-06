#include "model.h"

#include "dao/rdb/rdb_model.h"

namespace nx::cloud::db {

ModelFactory::ModelFactory():
    base_type([this](auto&&...) { return defaultFactoryFunction(); })
{
}

ModelFactory& ModelFactory::instance()
{
    static ModelFactory staticInstance;
    return staticInstance;
}

Model ModelFactory::defaultFactoryFunction()
{
    return dao::rdb::createModel();
}

} // namespace nx::cloud::db
