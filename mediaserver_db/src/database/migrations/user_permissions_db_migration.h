#pragma once

class QSqlDatabase;

namespace ec2 {
namespace db {

bool migrateV25UserPermissions(const QSqlDatabase& database);

bool fixCustomPermissionFlag(const QSqlDatabase& database);

}
}
