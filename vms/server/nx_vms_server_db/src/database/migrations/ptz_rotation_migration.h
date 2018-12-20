#pragma once

#include <QtSql/QSqlDatabase>
#include <nx/utils/log/log_level.h>

class QSqlDatabase;

namespace ec2 {
namespace migration {
namespace ptz {

bool addRotationToPresets(const nx::utils::log::Tag& logTag, QSqlDatabase& database);

} // namespace ptz
} // namespace migration
} // namespace ec2
