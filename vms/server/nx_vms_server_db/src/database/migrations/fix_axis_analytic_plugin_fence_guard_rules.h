#pragma once

class QSqlDatabase;

namespace ec2::db {

bool fixAxisAnalyticPluginFenceGuardRules(QSqlDatabase& db);

} // namespace ec2::db
