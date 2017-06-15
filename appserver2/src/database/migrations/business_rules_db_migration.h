#pragma once

class QSqlDatabase;

namespace ec2 {
namespace db {
bool migrateBusinessRulesToV23(const QSqlDatabase& database);
bool migrateBusinessRulesToV30(const QSqlDatabase& database);
bool migrateBusinessRulesToV31Alpha(const QSqlDatabase& database);
}
}