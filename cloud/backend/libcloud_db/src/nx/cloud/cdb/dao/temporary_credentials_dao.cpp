#include "temporary_credentials_dao.h"

#include <nx/fusion/model_functions.h>

#include "rdb/temporary_credentials_dao.h"

namespace nx::cdb::data {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (TemporaryAccountCredentialsEx),
    (sql_record),
    _Fields)

} // namespace nx::cdb::data

//-------------------------------------------------------------------------------------------------

namespace nx::cdb::dao {

TemporaryCredentialsDaoFactory::TemporaryCredentialsDaoFactory():
    base_type([this](){ return defaultFactoryFunction(); })
{
}

TemporaryCredentialsDaoFactory& TemporaryCredentialsDaoFactory::instance()
{
    static TemporaryCredentialsDaoFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractTemporaryCredentialsDao> 
    TemporaryCredentialsDaoFactory::defaultFactoryFunction()
{
    return std::make_unique<rdb::TemporaryCredentialsDao>();
}

} // namespace nx::cdb::dao
