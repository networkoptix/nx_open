#pragma once

#include <memory>

#include <nx/utils/basic_factory.h>

#include "dao/temporary_credentials_dao.h"

namespace nx::cloud::db {

struct Model
{
public:
    std::unique_ptr<dao::AbstractTemporaryCredentialsDao> temporaryCredentialsDao;
    // TODO: #ak Add everything else here.
};

//-------------------------------------------------------------------------------------------------

using ModelFactoryFunction = Model();

class ModelFactory:
    public nx::utils::BasicFactory<ModelFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<ModelFactoryFunction>;

public:
    ModelFactory();

    static ModelFactory& instance();

private:
    Model defaultFactoryFunction();
};

} // namespace nx::cloud::db
