#include "db_helper.h"
#include <QSqlQuery>
#include <QSqlRecord>
#include <QFile>
#include <QSqlError>

QnDbHelper::QnDbTransaction::QnDbTransaction(QSqlDatabase& database, QReadWriteLock& mutex): 
    m_database(database),
    m_mutex(mutex)
{

}

void QnDbHelper::QnDbTransaction::beginTran()
{
    m_mutex.lockForWrite();
    QSqlQuery query(m_database);
    query.exec(lit("BEGIN TRANSACTION"));
}

void QnDbHelper::QnDbTransaction::rollback()
{
    QSqlQuery query(m_database);
    query.exec(lit("ROLLBACK"));
    m_mutex.unlock();
}

void QnDbHelper::QnDbTransaction::commit()
{
    QSqlQuery query(m_database);
    query.exec(lit("COMMIT"));
    m_mutex.unlock();
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

void QnDbHelper::QnDbTransactionLocker::commit()
{
    m_tran->commit();
    m_committed = true;
}



QnDbHelper::QnDbHelper():
    m_tran(m_sdb, m_mutex)
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
    foreach(const QByteArray& singleCommand, commands)
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

void QnDbHelper::beginTran()
{
    m_tran.beginTran();
}

void QnDbHelper::rollback()
{
    m_tran.rollback();
}

void QnDbHelper::commit()
{
    m_tran.commit();
}

void QnDbHelper::addDatabase(const QString& fileName, const QString& dbname)
{
    m_sdb = QSqlDatabase::addDatabase(lit("QSQLITE"), dbname);
    m_sdb.setDatabaseName(fileName);
}
