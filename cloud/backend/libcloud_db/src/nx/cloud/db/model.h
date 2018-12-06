#pragma once

#include <memory>

#include <nx/utils/basic_factory.h>

#include "dao/temporary_credentials_dao.h"

namespace nx::cloud::db {

/**
 * This entity contains requirements to any DB implementation.
 * So, to implement persistent storage one should implement all abstract classes
 * used by this structure.
 */
struct Model
{
public:
    std::unique_ptr<dao::AbstractTemporaryCredentialsDao> temporaryCredentialsDao;
    // TODO: #ak Add everything else here.
};

//-------------------------------------------------------------------------------------------------

using ModelFactoryFunction = Model();

/**
 * By default, SQL DB implementation is used.
 */
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
