#ifndef __QN_DB_HELPER_H__
#define __QN_DB_HELPER_H__

#include <memory>

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
        virtual ~QnDbTransaction();
    protected:
        virtual bool beginTran();
        virtual void rollback();
        virtual bool commit();
    protected:
        friend class QnDbTransactionLocker;
        friend class QnDbHelper;

        QSqlDatabase& m_database;
        std::unique_ptr<QWriteLocker> m_writeLocker;

    private:
        QReadWriteLock& m_mutex;
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
    virtual QnDbTransaction* getTransaction() = 0;
    //const QnDbTransaction* getTransaction() const;

protected:
    bool isObjectExists(const QString& objectType, const QString& objectName, QSqlDatabase& database);
    void addDatabase(const QString& fileName, const QString& dbname);

protected:
    QSqlDatabase m_sdb;
    //QnDbTransaction m_tran;
    mutable QReadWriteLock m_mutex;
};

#endif // __QN_DB_HELPER_H__
