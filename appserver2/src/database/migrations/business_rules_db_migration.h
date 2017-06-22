#pragma once

class QSqlDatabase;

namespace ec2 {
namespace db {

bool migrateBusinessRulesToV23(const QSqlDatabase& database);
bool migrateBusinessRulesToV30(const QSqlDatabase& database);

// 3.0 to 3.1 Alpha.
bool migrateBusinessRulesToV31Alpha(const QSqlDatabase& database);

// 3.1 Alpha to 3.1 Beta.
bool migrateBusinessActionsAllUsers(const QSqlDatabase& database);

} // namespace db
} // namespace ec2
