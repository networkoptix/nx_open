#pragma once

class QSqlDatabase;

namespace ec2::db {

bool removeCameraAdvancedParamsTransactions(const QSqlDatabase& database);

} // namespace ec2::db
