#pragma once

#include <QtSql/QSqlDatabase>

namespace ec2 {
namespace database {

namespace api {
class QueryContext;
}

namespace migrations {

/**
 * Web page with support link must be added by default (if exists in current customization)
 */
bool addDefaultWebpages(ec2::database::api::QueryContext* context);

} // namespace migrations
} // namespace database
} // namespace ec2
