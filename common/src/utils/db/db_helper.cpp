#include "db_helper.h"
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
        if (singleCommand.trimmed().isEmpty())
            continue;
        QSqlQuery ddlQuery(database);
        ddlQuery.prepare(QString::fromUtf8(singleCommand));
        if (!ddlQuery.exec()) {
            qWarning() << "can't create tables for sqlLite database:" << ddlQuery.lastError().text() << ". Query was: " << singleCommand;
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
