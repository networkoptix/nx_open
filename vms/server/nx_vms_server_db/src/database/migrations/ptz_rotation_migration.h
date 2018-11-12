#pragma once

class QSqlDatabase;

namespace ec2 {
namespace migration {
namespace ptz {

bool addRotationToPresets(QSqlDatabase& database);

} // namespace ptz
} // namespace migration
} // namespace ec2
