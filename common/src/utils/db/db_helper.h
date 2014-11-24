#ifndef __QN_DB_HELPER_H__
#define __QN_DB_HELPER_H__

#include <QSqlDatabase>
#include <QReadWriteLock>

class QSqlDatabase;

class QnDbHelper
{
public:
    class QnDbTransaction
    {
    public:
        QnDbTransaction(QSqlDatabase& database, QReadWriteLock& mutex);

    private:
        friend class QnDbTransactionLocker;
        friend class QnDbHelper;

        QSqlDatabase& m_database;
        QReadWriteLock& m_mutex;

        void beginTran();
        void rollback();
        bool commit();
    };

    class QnDbTransactionLocker
    {
    public:
        QnDbTransactionLocker(QnDbTransaction* tran);
        ~QnDbTransactionLocker();
        bool commit();

    private:
        bool m_committed;
        QnDbTransaction* m_tran;
    };

    QnDbHelper();

    bool execSQLFile(const QString& fileName, QSqlDatabase& database);
    bool execSQLQuery(const QString& query, QSqlDatabase& database);
    QnDbTransaction* getTransaction();
    const QnDbTransaction* getTransaction() const;

protected:
    bool isObjectExists(const QString& objectType, const QString& objectName, QSqlDatabase& database);
    void addDatabase(const QString& fileName, const QString& dbname);

protected:
    QSqlDatabase m_sdb;
    QnDbTransaction m_tran;
    mutable QReadWriteLock m_mutex;

private:
    void beginTran();
    void rollback();
    void commit();
};

#endif // __QN_DB_HELPER_H__
