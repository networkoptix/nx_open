#pragma once

class QSqlDatabase;

namespace ec2
{
    namespace db
    {
        bool migrateBusinessEvents(const QSqlDatabase& database);
    }
}