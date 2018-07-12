#pragma once

class QSqlDatabase;

namespace ec2
{
    namespace db
    {
        bool migrateAccessibleResources(const QSqlDatabase& database);
    }
}