#ifndef __QN_DB_HELPER_H__
#define __QN_DB_HELPER_H__

#include <QSqlDatabase>
#include <QReadWriteLock>

class QSqlDatabase;

class QnDbHelper
{
public:
    QnDbHelper();

    bool execSQLFile(const QString& fileName, QSqlDatabase& database);
    bool execSQLQuery(const QString& query, QSqlDatabase& database);

    void beginTran();
    void rollback();
    void commit();

protected:
    class QnDbTransactionLocker;

    class QnDbTransaction
    {
    public:
        QnDbTransaction(QSqlDatabase& database, QReadWriteLock& mutex);

        void beginTran();
        void rollback();
        void commit();
    private:
        friend class QnDbTransactionLocker;

    private:
        QSqlDatabase& m_database;
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

    bool isObjectExists(const QString& objectType, const QString& objectName, QSqlDatabase& database);
    void addDatabase(const QString& fileName, const QString& dbname);
protected:
    QSqlDatabase m_sdb;
    QnDbTransaction m_tran;
    mutable QReadWriteLock m_mutex;
};

#endif // __QN_DB_HELPER_H__
