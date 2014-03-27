#ifndef __QN_DB_HELPER_H__
#define __QN_DB_HELPER_H__

#include <QSqlDatabase>
#include <QReadWriteLock>

class QSqlDatabase;

class QnDbHelper
{
public:
    QnDbHelper();
protected:
    bool execSQLFile(const QString& fileName);
    bool execSQLQuery(const QString& query);
protected:
    class QnDbTransactionLocker;

    class QnDbTransaction
    {
    public:
        QnDbTransaction(QSqlDatabase& m_sdb, QReadWriteLock& mutex);
    private:
        friend class QnDbTransactionLocker;

        void beginTran();
        void rollback();
        void commit();
    private:
        QSqlDatabase& m_sdb;
        QReadWriteLock& m_mutex;
    };

    class QnDbTransactionLocker
    {
    public:
        QnDbTransactionLocker(QnDbTransaction* tran);
        ~QnDbTransactionLocker();
        void commit();
    private:
        bool m_committed;
        QnDbTransaction* m_tran;
    };

    bool isObjectExists(const QString& objectType, const QString& objectName);

protected:
    QSqlDatabase m_sdb;
    QnDbTransaction m_tran;
    mutable QReadWriteLock m_mutex;
};

#endif // __QN_DB_HELPER_H__
