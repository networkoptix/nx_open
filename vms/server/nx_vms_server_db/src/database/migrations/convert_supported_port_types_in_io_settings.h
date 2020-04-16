#pragma once

class QSqlDatabase;

namespace ec2::detail {

bool convertSupportedPortTypesInIoSettings(const QSqlDatabase& database, bool* converted);

} // namespace ec2::detail
