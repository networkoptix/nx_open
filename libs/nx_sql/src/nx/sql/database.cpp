// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "database.h"

namespace nx::sql {

nx::Mutex Database::m_mutex;

QSqlDatabase Database::addDatabase(const QString& type, const QString& connectionName)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return QSqlDatabase::addDatabase(type, connectionName);
}

void Database::removeDatabase(const QString& connectionName)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace nx::sql
