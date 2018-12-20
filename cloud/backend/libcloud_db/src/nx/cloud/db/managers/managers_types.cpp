#include "managers_types.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::db {

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

api::ResultCode ec2ResultToResult(clusterdb::engine::ResultCode resultCode)
{
    switch (resultCode)
    {
        case clusterdb::engine::ResultCode::ok:
            return api::ResultCode::ok;
        case clusterdb::engine::ResultCode::partialContent:
            return api::ResultCode::partialContent;
        case clusterdb::engine::ResultCode::dbError:
            return api::ResultCode::dbError;
        case clusterdb::engine::ResultCode::retryLater:
            return api::ResultCode::retryLater;
        case clusterdb::engine::ResultCode::notFound:
            return api::ResultCode::notFound;
        case clusterdb::engine::ResultCode::badRequest:
            return api::ResultCode::badRequest;
        default:
            return api::ResultCode::unknownError;
    }
}

} // namespace nx::cloud::db

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::db, EntityType,
    (nx::cloud::db::EntityType::none, "none")
    (nx::cloud::db::EntityType::module, "module")
    (nx::cloud::db::EntityType::account, "account")
    (nx::cloud::db::EntityType::system, "system")
    (nx::cloud::db::EntityType::subscription, "subscription")
    (nx::cloud::db::EntityType::product, "product")
    (nx::cloud::db::EntityType::maintenance, "maintenance")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::db, DataActionType,
    (nx::cloud::db::DataActionType::fetch, "fetch")
    (nx::cloud::db::DataActionType::insert, "insert")
    (nx::cloud::db::DataActionType::update, "update")
    (nx::cloud::db::DataActionType::delete_, "delete")
)
