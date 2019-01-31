#pragma once

class QSqlDatabase;

namespace ec2 {
namespace db {

bool migrateRulesToV23(const QSqlDatabase& database);
bool migrateRulesToV30(const QSqlDatabase& database);

// 3.0 to 3.1 Alpha.
bool migrateRulesToV31Alpha(const QSqlDatabase& database);

// 3.1 Alpha to 3.1 Beta.
bool migrateActionsAllUsers(const QSqlDatabase& database);
bool migrateEventsAllUsers(const QSqlDatabase& database);

} // namespace db
} // namespace ec2
