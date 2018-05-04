#pragma once

#include <nx/utils/db/query_context.h>

#include <database/migrations/add_history_attributes_to_transaction.h>

namespace nx {
namespace data_sync_engine {
namespace migration {
namespace addHistoryToTransaction {

namespace before {
typedef ::ec2::migration::add_history::before::QnAbstractTransaction QnAbstractTransaction;
} // namespace before

namespace after {
typedef ::ec2::migration::add_history::after::QnAbstractTransaction QnAbstractTransaction;
} // namespace before

NX_DATA_SYNC_ENGINE_API nx::utils::db::DBResult migrate(
    nx::utils::db::QueryContext* const queryContext);

} // namespace addHistoryToTransaction
} // namespace migration
} // namespace data_sync_engine
} // namespace nx
