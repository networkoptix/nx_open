#pragma once

class QSqlDatabase;

namespace ec2 {

namespace detail {
    class QnDbManager;
}

namespace db {

bool migrateAccessRightsToUbjsonFormat(QSqlDatabase& database, detail::QnDbManager* db);

} // namespace db
} // namespace ec2
