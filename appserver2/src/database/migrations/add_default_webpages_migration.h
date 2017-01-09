#pragma once

#include <QtSql/QSqlDatabase>

namespace ec2 {
namespace database {
namespace migrations {

/**
 * Web page with support link must be added by default (if exists in current customization)
 */
bool addDefaultWebpages(const QSqlDatabase& database);

} // namespace migrations
} // namespace database
} // namespace ec2
