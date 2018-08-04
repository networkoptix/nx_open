#include "managers_types.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cdb {

api::ResultCode dbResultToApiResult(nx::sql::DBResult dbResult)
{
    switch (dbResult)
    {
        case nx::sql::DBResult::ok:
        case nx::sql::DBResult::endOfData:
            return api::ResultCode::ok;

        case nx::sql::DBResult::notFound:
            return api::ResultCode::notFound;

        case nx::sql::DBResult::cancelled:
            return api::ResultCode::retryLater;

        case nx::sql::DBResult::ioError:
        case nx::sql::DBResult::statementError:
        case nx::sql::DBResult::connectionError:
            return api::ResultCode::dbError;

        case nx::sql::DBResult::retryLater:
            return api::ResultCode::retryLater;

        case nx::sql::DBResult::uniqueConstraintViolation:
            return api::ResultCode::alreadyExists;

        case nx::sql::DBResult::logicError:
            return api::ResultCode::unknownError;
    }

    return api::ResultCode::dbError;
}

api::ResultCode ec2ResultToResult(data_sync_engine::ResultCode resultCode)
{
    switch (resultCode)
    {
        case data_sync_engine::ResultCode::ok:
            return api::ResultCode::ok;
        case data_sync_engine::ResultCode::partialContent:
            return api::ResultCode::partialContent;
        case data_sync_engine::ResultCode::dbError:
            return api::ResultCode::dbError;
        case data_sync_engine::ResultCode::retryLater:
            return api::ResultCode::retryLater;
        case data_sync_engine::ResultCode::notFound:
            return api::ResultCode::notFound;
        case data_sync_engine::ResultCode::badRequest:
            return api::ResultCode::badRequest;
        default:
            return api::ResultCode::unknownError;
    }
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
    (nx::cdb::DataActionType::delete_, "delete")
)
