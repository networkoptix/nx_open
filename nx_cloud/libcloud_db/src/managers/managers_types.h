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
#include <utils/db/types.h>
#include <nx/fusion/fusion/fusion_fwd.h>

#include <cdb/result_code.h>


namespace nx {
namespace cdb {

api::ResultCode fromDbResultCode( nx::db::DBResult );

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
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((EntityType), (lexical))

enum class DataActionType
{
    fetch,
    insert,
    update,
    _delete
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(DataActionType)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((DataActionType), (lexical))

}   //cdb
}   //nx

#endif  //CLOUD_DB_MANAGERS_TYPES_H
