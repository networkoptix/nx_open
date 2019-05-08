#include "database.h"

namespace nx::sql {

QnMutex Database::m_mutex;

QSqlDatabase Database::addDatabase(const QString& type, const QString& connectionName)
{
    QnMutexLocker lock(&m_mutex);
    return QSqlDatabase::addDatabase(type, connectionName);
}

void Database::removeDatabase(const QString& connectionName)
{
    QnMutexLocker lock(&m_mutex);
    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace nx::sql