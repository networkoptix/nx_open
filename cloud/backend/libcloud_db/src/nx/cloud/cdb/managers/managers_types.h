#pragma once

#include <map>
#include <string>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/sql/types.h>

#include <nx/cloud/cdb/api/result_code.h>
#include <nx/data_sync_engine/result_code.h>

namespace nx::cdb {

api::ResultCode dbResultToApiResult(nx::sql::DBResult);

api::ResultCode ec2ResultToResult(data_sync_engine::ResultCode resultCode);

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

} // namespace nx::cdb

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cdb::EntityType), (lexical))
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cdb::DataActionType), (lexical))
