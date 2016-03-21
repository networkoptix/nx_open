#ifndef __QN_DB_HELPER_H__
#define __QN_DB_HELPER_H__

#include <QtSql/QSqlDatabase>

#include <utils/thread/mutex.h>

class QSqlDatabase;

class QnDbHelper
{
public:
    class QnDbTransaction
    {
    public:
        QnDbTransaction(QSqlDatabase& database, QnReadWriteLock& mutex);
        virtual ~QnDbTransaction();
    protected:
        virtual bool beginTran();
        virtual void rollback();
        virtual bool commit();
    protected:
        friend class QnDbTransactionLocker;
        friend class QnDbHelper;

        QSqlDatabase& m_database;
        QnReadWriteLock& m_mutex;
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
    virtual ~QnDbHelper();

    bool execSQLFile(const QString& fileName, QSqlDatabase& database) const;
    bool execSQLQuery(const QString& query, QSqlDatabase& database, const char* details) const;
    bool execSQLQuery(QSqlQuery *query, const char* details) const;

    virtual QnDbTransaction* getTransaction() = 0;
    //const QnDbTransaction* getTransaction() const;

    bool applyUpdates(const QString &dirName);
    virtual bool beforeInstallUpdate(const QString& updateName);
    virtual bool afterInstallUpdate(const QString& updateName);

protected:
    bool isObjectExists(const QString& objectType, const QString& objectName, QSqlDatabase& database);
    void addDatabase(const QString& fileName, const QString& dbname);
    void removeDatabase();
protected:
    QSqlDatabase m_sdb;
    mutable QnReadWriteLock m_mutex;
    QString m_connectionName;
};

#endif // __QN_DB_HELPER_H__
