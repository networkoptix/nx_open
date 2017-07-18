#pragma once

#include <nx/utils/db/query_context.h>

#include <database/migrations/add_history_attributes_to_transaction.h>

namespace nx {
namespace cdb {
namespace ec2 {
namespace migration {
namespace addHistoryToTransaction {

namespace before {
typedef ::ec2::migration::add_history::before::QnAbstractTransaction QnAbstractTransaction;
} // namespace before

namespace after {
typedef ::ec2::migration::add_history::after::QnAbstractTransaction QnAbstractTransaction;
} // namespace before

nx::utils::db::DBResult migrate(nx::utils::db::QueryContext* const queryContext);

} // namespace addHistoryToTransaction
} // namespace migration
} // namespace ec2
} // namespace cdb
} // namespace nx
