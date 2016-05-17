#pragma once

class QSqlDatabase;

namespace ec2
{
    namespace db
    {
        bool migrateUserPermissions(const QSqlDatabase& database);
    }
}