#include "db_helper.h"

#include <QtCore/QCoreApplication>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QFile>
#include <QSqlError>

//TODO #AK QnDbTransaction is a bad name for this class since it actually lives beyond DB transaction 
    //and no concurrent transactions supported. Maybe QnDbConnection?
QnDbHelper::QnDbTransaction::QnDbTransaction(QSqlDatabase& database, QReadWriteLock& mutex): 
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

void QnDbHelper::QnDbTransaction::rollback()
{
    m_database.rollback();
    //TODO #ak handle rollback error?
    m_mutex.unlock();
}

bool QnDbHelper::QnDbTransaction::commit()
{
    bool rez = m_database.commit();
    if (rez)
        m_mutex.unlock();
    else
        qWarning() << "Commit failed:" << m_database.lastError(); // do not unlock mutex. Rollback is expected
    return rez;
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
    return m_committed;
}

QnDbHelper::QnDbHelper()
{

}

QnDbHelper::~QnDbHelper()
{

}

QList<QByteArray> quotedSplit(const QByteArray& data)
{
    QList<QByteArray> result;
    const char* curPtr = data.data();
    const char* prevPtr = curPtr;
    const char* end = curPtr + data.size();
    bool inQuote1 = false;
    bool inQuote2 = false;
    for (;curPtr < end; ++curPtr) {
        if (*curPtr == '\'')
            inQuote1 = !inQuote1;
        else if (*curPtr == '\"')
            inQuote2 = !inQuote2;
        else if (*curPtr == ';' && !inQuote1 && !inQuote2)
        {
            //*curPtr = 0;
            result << QByteArray::fromRawData(prevPtr, curPtr - prevPtr);
            prevPtr = curPtr+1;
        }
    }

    return result;
}

bool QnDbHelper::execSQLQuery(const QString& queryStr, QSqlDatabase& database)
{
    QSqlQuery query(database);
    query.prepare(queryStr);
    bool rez = query.exec(queryStr);
    if (!rez)
        qWarning() << "Cant exec query:" << query.lastError();
    return rez;
}

bool QnDbHelper::execSQLFile(const QString& fileName, QSqlDatabase& database)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return false;
    QByteArray data = file.readAll();
    QList<QByteArray> commands = quotedSplit(data);
#ifdef DB_DEBUG
    int n = commands.size();
    qDebug() << "creating db" << n << "commands queued";
    int i = 0;
#endif // DB_DEBUG
    for(const QByteArray& singleCommand: commands)
    {
#ifdef DB_DEBUG
        qDebug() << QString(QLatin1String("processing command %1 of %2")).arg(++i).arg(n);
#endif // DB_DEBUG
        QString command = QString::fromUtf8(singleCommand).trimmed();
        if (command.isEmpty())
            continue;
        QSqlQuery ddlQuery(database);
        ddlQuery.prepare(command);
        if (!ddlQuery.exec()) {
            qWarning() << "Error while executing SQL file" << fileName;
            qWarning() << "Query was:" << command;
            qWarning() << "Error:" << ddlQuery.lastError().text();
            return false;
        }
    }
    return true;
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
    m_sdb = QSqlDatabase::addDatabase(lit("QSQLITE"), dbname);
    m_sdb.setDatabaseName(fileName);
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
            if (!beforeInstallUpdate(fileName))
                return false;
            if (!execSQLFile(fileName, m_sdb))
                return false;
            if (!afterInstallUpdate(fileName))
                return false;

            QSqlQuery insQuery(m_sdb);
            insQuery.prepare(lit("INSERT INTO south_migrationhistory (app_name, migration, applied) values(?, ?, ?)"));
            insQuery.addBindValue(qApp->applicationName());
            insQuery.addBindValue(fileName);
            insQuery.addBindValue(QDateTime::currentDateTime());
            if (!insQuery.exec()) {
                qWarning() << Q_FUNC_INFO << __LINE__ << insQuery.lastError();
                return false;
            }
        }
    }

    return true;
}

bool QnDbHelper::beforeInstallUpdate(const QString& updateName) {
    Q_UNUSED(updateName);
    return true;
}

bool QnDbHelper::afterInstallUpdate(const QString& updateName) {
    Q_UNUSED(updateName);
    return true;
}
