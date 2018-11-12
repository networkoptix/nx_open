#include "user_authentication_data_object_factory.h"

#include <functional>

#include <nx/utils/std/cpp14.h>

#include "rdb/dao_rdb_user_authentication.h"

namespace nx {
namespace cdb {
namespace dao {

UserAuthenticationDataObjectFactory::UserAuthenticationDataObjectFactory():
    base_type(std::bind(&UserAuthenticationDataObjectFactory::defaultFactoryFunction, this))
{
}

UserAuthenticationDataObjectFactory& UserAuthenticationDataObjectFactory::instance()
{
    static UserAuthenticationDataObjectFactory staticInstance;
    return staticInstance;
}

std::unique_ptr<AbstractUserAuthentication>
    UserAuthenticationDataObjectFactory::defaultFactoryFunction()
{
    return std::make_unique<rdb::UserAuthentication>();
}

} // namespace dao
} // namespace cdb
} // namespace nx
