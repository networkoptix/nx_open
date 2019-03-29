#include "database.h"
#include <nx/utils/thread/mutex.h>

namespace nx::sql {

QSqlDatabase Database::addDatabase(const QString& type, const QString& connectionName)
{
    static QnMutex m_mutex;
    QnMutexLocker lock(&m_mutex);

    return QSqlDatabase::addDatabase(type, connectionName);
}

} // namespace nx::sql