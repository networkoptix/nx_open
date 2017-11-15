#pragma once

#include <map>
#include <string>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/db/types.h>

#include <nx/cloud/cdb/api/result_code.h>

namespace nx {
namespace cdb {

api::ResultCode dbResultToApiResult(nx::utils::db::DBResult);

enum class EntityType
{
    none,
    module,
    account,
    system,
    subscription,
    product,
    maintenance,
    //...
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(EntityType)

enum class DataActionType
{
    fetch,
    insert,
    update,
    delete_
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(DataActionType)

} // namespace cdb
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cdb::EntityType), (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cdb::DataActionType), (lexical))
