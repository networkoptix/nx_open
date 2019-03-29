#pragma once

#include <QtSql/QSqlDatabase>
#include <nx/utils/thread/mutex.h>

namespace nx::sql {

class NX_SQL_API Database
{
public:
    /*
     * This function do the as QSqlDataBase::addDatabase but under mutex.
     * We surmise a bug in Qt that leads to QApplication got stuck in destructor
     * in case of parallel addDatabase calls.
     */
    static QSqlDatabase addDatabase(const QString& type, const QString& connectionName);

    static void removeDatabase(const QString& connectionName);
private:
    static QnMutex m_mutex;
};

} // namespace nx::sql
