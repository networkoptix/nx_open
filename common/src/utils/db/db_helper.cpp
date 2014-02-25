#include "db_helper.h"
#include <QSqlQuery>
#include <QSqlRecord>
#include <QFile>
#include <QSqlError>

QnDbHelper::QnDbTransaction::QnDbTransaction(QSqlDatabase& sdb, QReadWriteLock& mutex): 
    m_sdb(sdb),
    m_mutex(mutex)
{

}

void QnDbHelper::QnDbTransaction::beginTran()
{
    m_mutex.lockForWrite();
    QSqlQuery query(m_sdb);
    //query.exec("BEGIN TRANSACTION");
}

void QnDbHelper::QnDbTransaction::rollback()
{
    QSqlQuery query(m_sdb);
    //query.exec("ROLLBACK");
    m_mutex.unlock();
}

void QnDbHelper::QnDbTransaction::commit()
{
    QSqlQuery query(m_sdb);
    //query.exec("COMMIT");
    m_mutex.unlock();
}

QnDbHelper::QnDbTransactionLocker::QnDbTransactionLocker(QnDbTransaction* tran): 
    m_tran(tran), 
    m_committed(false)
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

bool QnDbHelper::execSQLFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return false;
    QByteArray data = file.readAll();
    foreach(const QByteArray& singleCommand, quotedSplit(data))
    {
        if (singleCommand.trimmed().isEmpty())
            continue;
        QSqlQuery ddlQuery(m_sdb);
        ddlQuery.prepare(QString::fromUtf8(singleCommand));
        if (!ddlQuery.exec()) {
            qWarning() << "can't create tables for sqlLite database:" << ddlQuery.lastError().text();
            return false;
        }
    }
    return true;
}


bool QnDbHelper::isObjectExists(const QString& objectType, const QString& objectName)
{
    QSqlQuery tableList(m_sdb);
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

