/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_MANAGERS_TYPES_H
#define CLOUD_DB_MANAGERS_TYPES_H

#include <map>
#include <string>

#include <QtCore/QVariant>  //TODO #ak maybe boost::variant?
#include <QtCore/QUrlQuery>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/db/types.h>
#include <nx/fusion/fusion/fusion_fwd.h>

#include <nx/cloud/cdb/api/result_code.h>


namespace nx {
namespace cdb {

api::ResultCode dbResultToApiResult( nx::utils::db::DBResult );

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

#endif  //CLOUD_DB_MANAGERS_TYPES_H
