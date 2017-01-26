/**********************************************************
* Aug 18, 2015
* a.kolesnikov
***********************************************************/

#include "managers_types.h"

#include <nx/fusion/model_functions.h>


namespace nx {
namespace cdb {

api::ResultCode dbResultToApiResult( nx::db::DBResult dbResult )
{
    switch( dbResult )
    {
        case nx::db::DBResult::ok:
            return api::ResultCode::ok;

        case nx::db::DBResult::notFound:
            return api::ResultCode::notFound;

        case nx::db::DBResult::cancelled:
            return api::ResultCode::retryLater;

        case nx::db::DBResult::ioError:
        case nx::db::DBResult::statementError:
        case nx::db::DBResult::connectionError:
            return api::ResultCode::dbError;

        case nx::db::DBResult::retryLater:
            return api::ResultCode::retryLater;

        case nx::db::DBResult::uniqueConstraintViolation:
            return api::ResultCode::alreadyExists;
    }

    return api::ResultCode::dbError;
}

} // namespace cdb
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb, EntityType,
    (nx::cdb::EntityType::none, "none")
    (nx::cdb::EntityType::module, "module")
    (nx::cdb::EntityType::account, "account")
    (nx::cdb::EntityType::system, "system")
    (nx::cdb::EntityType::subscription, "subscription")
    (nx::cdb::EntityType::product, "product")
    (nx::cdb::EntityType::maintenance, "maintenance")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb, DataActionType,
    (nx::cdb::DataActionType::fetch, "fetch")
    (nx::cdb::DataActionType::insert, "insert")
    (nx::cdb::DataActionType::update, "update")
    (nx::cdb::DataActionType::_delete, "delete")
)
