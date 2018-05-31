#pragma once

class QSqlDatabase;

namespace ec2 {
namespace db {

bool migrateRecordingThresholds(const QSqlDatabase& database);

} // namespace db
} // namespace ec2
