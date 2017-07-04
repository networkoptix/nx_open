#include "db_helper.h"

#include <QtCore/QCoreApplication>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QtSql/QSqlError>

#include <nx/utils/db/sql_query_execution_helper.h>
#include <nx/utils/log/log.h>
#include <nx/kit/ini_config.h>

namespace  {

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("db_helper.ini") { reload(); }

    NX_INI_STRING("", tuneDb, "SQL stataments ceparated by ';' to execute after DB open.");
}
config;

} // namespace

//TODO #AK QnDbTransaction is a bad name for this class since it actually lives beyond DB transaction
    //and no concurrent transactions supported. Maybe QnDbConnection?
QnDbHelper::QnDbTransaction::QnDbTransaction(QSqlDatabase& database, QnReadWriteLock& mutex):
    m_database(database),
    m_mutex(mutex)
{
}

QnDbHelper::QnDbTransaction::~QnDbTransaction()
{
}

bool QnDbHelper::QnDbTransaction::beginTran()
{
    m_mutex.lockForWrite();
    if( !m_database.transaction() )
    {
        //TODO #ak ignoring this error since calling party thinks it always succeeds
        //m_mutex.unlock();
        //return false;
    }
    return true;
}

bool QnDbHelper::tuneDBAfterOpen(QSqlDatabase* const sqlDb)
{
    QSqlQuery enableWalQuery(*sqlDb);
    enableWalQuery.prepare(lit("PRAGMA journal_mode = WAL"));
    if( !enableWalQuery.exec() )
    {
        qWarning() << "Failed to enable WAL mode on sqlLite database!" << enableWalQuery.lastError().text();
        return false;
    }

    QSqlQuery enableFKQuery(*sqlDb);
    enableFKQuery.prepare(lit("PRAGMA foreign_keys = ON"));
    if( !enableFKQuery.exec() )
    {
        qWarning() << "Failed to enable FK support on sqlLite database!" << enableFKQuery.lastError().text();
        return false;
    }

#if 0
    QSqlQuery additionTuningQuery(*sqlDb);
    additionTuningQuery.prepare(lit("PRAGMA synchronous=OFF"));
    if (!additionTuningQuery.exec())
    {
        qWarning() << "Failed to turn off synchronous mode on sqlLite database!" << additionTuningQuery.lastError().text();
        return false;
    }
#endif

    const auto tuneQueries = QString::fromLatin1(config.tuneDb)
        .split(QChar::fromLatin1(';'), QString::SkipEmptyParts);
    for (const auto& queryLine: tuneQueries)
    {
        QSqlQuery query(*sqlDb);
        query.prepare(queryLine);
        if (!query.exec())
        {
            qWarning() << "Failed to execute" << queryLine << "on sqlLite database!" << query.lastError().text();
            return false;
        }
    }

    return true;
}


void QnDbHelper::QnDbTransaction::rollback()
{
    m_database.rollback();
    //TODO #ak handle rollback error?
    m_mutex.unlock();
}

bool QnDbHelper::QnDbTransaction::commit()
{
    bool rez = dbCommit(QString());
    if (rez)
    {
        // Commit only on success, otherwise rollback is expected.
        m_mutex.unlock();
    }

    return rez;
}

bool QnDbHelper::QnDbTransaction::dbCommit(const QString& event)
{
    if (m_database.commit())
    {
        NX_VERBOSE(this, lm("Successful commit in %1 on (%2)").args(
            m_database.databaseName(), event));
        return true;
    }
    else
    {
        NX_VERBOSE(this, lm("Failed commit in %1 on (%2): %3").args(
            m_database.databaseName(), event, m_database.lastError()));
        return false;
    }
}

QnDbHelper::QnDbTransactionLocker::QnDbTransactionLocker(QnDbTransaction* tran):
    m_committed(false),
    m_tran(tran)
{
    m_tran->beginTran();
}

QnDbHelper::QnDbTransactionLocker::~QnDbTransactionLocker()
{
    if (!m_committed)
        m_tran->rollback();
}

bool QnDbHelper::QnDbTransactionLocker::commit()
{
    m_committed = m_tran->commit();
    if (!m_committed)
    {
        NX_LOG(lit("%1. Commit failed: %2").
            arg(m_tran->m_database.databaseName()).
            arg(m_tran->m_database.lastError().text()), cl_logERROR);
    }
    return m_committed;
}

QnDbHelper::QnDbHelper()
{

}

QnDbHelper::~QnDbHelper()
{
    removeDatabase();
}

bool QnDbHelper::isObjectExists(const QString& objectType, const QString& objectName, QSqlDatabase& database)
{
    QSqlQuery tableList(database);
    QString request;
    request = QString(lit("SELECT * FROM sqlite_master WHERE type='%1' and name='%2'")).arg(objectType).arg(objectName);
    tableList.prepare(request);
    if (!tableList.exec())
        return false;
    int fieldNo = tableList.record().indexOf(lit("name"));
    if (!tableList.next())
        return false;
    QString value = tableList.value(fieldNo).toString();
    return !value.isEmpty();
}

void QnDbHelper::addDatabase(const QString& fileName, const QString& dbname)
{
    QFileInfo dirInfo(fileName);
    if (!QDir().mkpath(dirInfo.absoluteDir().path()))
        NX_LOG(lit("can't create folder for sqlLite database!\n %1").arg(fileName), cl_logERROR);
    m_connectionName = dbname;
    m_sdb = QSqlDatabase::addDatabase(lit("QSQLITE"), dbname);
    m_sdb.setDatabaseName(fileName);
}

void QnDbHelper::removeDatabase()
{
    m_sdb = QSqlDatabase();
    if (!m_connectionName.isEmpty())
        QSqlDatabase::removeDatabase(m_connectionName);
}

bool QnDbHelper::applyUpdates(const QString &dirName) {

    if (!isObjectExists(lit("table"), lit("south_migrationhistory"), m_sdb)) {
        QSqlQuery ddlQuery(m_sdb);
        ddlQuery.prepare(lit(
            "CREATE TABLE \"south_migrationhistory\" (\
            \"id\" integer NOT NULL PRIMARY KEY autoincrement,\
            \"app_name\" varchar(255) NOT NULL,\
            \"migration\" varchar(255) NOT NULL,\
            \"applied\" datetime NOT NULL);"
            ));
        if (!ddlQuery.exec())
            return false;
    }

    QSqlQuery existsUpdatesQuery(m_sdb);
    existsUpdatesQuery.prepare(lit("SELECT migration from south_migrationhistory"));
    if (!existsUpdatesQuery.exec())
        return false;
    QStringList existUpdates;
    while (existsUpdatesQuery.next())
        existUpdates << existsUpdatesQuery.value(0).toString();


    QDir dir(dirName);
    for(const QFileInfo& entry: dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files, QDir::Name))
    {
        QString fileName = entry.absoluteFilePath();
        if (!existUpdates.contains(fileName))
        {
            NX_LOG(lit("Applying SQL update %1").arg(fileName), cl_logDEBUG1);
            if (!beforeInstallUpdate(fileName))
                return false;
            if (!nx::utils::db::SqlQueryExecutionHelper::execSQLFile(fileName, m_sdb))
                return false;
            if (!afterInstallUpdate(fileName))
                return false;

            QSqlQuery insQuery(m_sdb);
            insQuery.prepare(lit("INSERT INTO south_migrationhistory (app_name, migration, applied) values(?, ?, ?)"));
            insQuery.addBindValue(qApp->applicationName());
            insQuery.addBindValue(fileName);
            insQuery.addBindValue(QDateTime::currentDateTime());
            if (!insQuery.exec())
            {
                NX_LOG(lit("Can not register SQL update. SQL error: %1").
                    arg(insQuery.lastError().text()), cl_logERROR);
                return false;
            }
        }
    }

    return true;
}

bool QnDbHelper::beforeInstallUpdate(const QString& updateName)
{
    Q_UNUSED(updateName);
    return true;
}

bool QnDbHelper::afterInstallUpdate(const QString& updateName)
{
    Q_UNUSED(updateName);
    return true;
}
