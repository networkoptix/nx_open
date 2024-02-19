// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "database.h"

namespace nx::sql {

static nx::Mutex& mutex()
{
    static nx::Mutex instance;
    return instance;
}

QSqlDatabase Database::addDatabase(const QString& type, const QString& connectionName)
{
    NX_MUTEX_LOCKER lock(&mutex());
    return QSqlDatabase::addDatabase(type, connectionName);
}

void Database::removeDatabase(const QString& connectionName)
{
    NX_MUTEX_LOCKER lock(&mutex());
    QSqlDatabase::removeDatabase(connectionName);
}

bool Database::openDatabase(QSqlDatabase* database)
{
    NX_MUTEX_LOCKER lock(&mutex());
    return database->open();
}

} // namespace nx::sql
