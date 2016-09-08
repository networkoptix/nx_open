/**********************************************************
* Aug 18, 2015
* a.kolesnikov
***********************************************************/

#include "managers_types.h"

#include <nx/fusion/model_functions.h>


namespace nx {
namespace cdb {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(EntityType,
    (EntityType::none, "none")
    (EntityType::module, "module")
    (EntityType::account, "account")
    (EntityType::system, "system")
    (EntityType::subscription, "subscription")
    (EntityType::product, "product")
    (EntityType::maintenance, "maintenance")
    )

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(DataActionType,
    (DataActionType::fetch, "fetch")
    (DataActionType::insert, "insert")
    (DataActionType::update, "update")
    (DataActionType::_delete, "delete")
    )


api::ResultCode fromDbResultCode( nx::db::DBResult dbResult )
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
            return api::ResultCode::dbError;

        case nx::db::DBResult::retryLater:
            return api::ResultCode::retryLater;
    }

    return api::ResultCode::dbError;
}

}   //cdb
}   //nx
