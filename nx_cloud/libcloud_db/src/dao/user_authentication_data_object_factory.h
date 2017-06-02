#pragma once

#include <nx/utils/basic_factory.h>

#include "abstract_user_authentication_data_object.h"

namespace nx {
namespace cdb {
namespace dao {

using UserAuthenticationDataObjectFactoryFunction =
    std::unique_ptr<AbstractUserAuthentication>();

class UserAuthenticationDataObjectFactory:
    public nx::utils::BasicFactory<UserAuthenticationDataObjectFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<UserAuthenticationDataObjectFactoryFunction>;

public:
    UserAuthenticationDataObjectFactory();

    static UserAuthenticationDataObjectFactory& instance();

private:
    std::unique_ptr<AbstractUserAuthentication> defaultFactoryFunction();
};

} // namespace dao
} // namespace cdb
} // namespace nx
