#ifndef __QN_DB_HELPER_H__
#define __QN_DB_HELPER_H__

#include <QtSql/QSqlDatabase>

#include <nx/utils/thread/mutex.h>

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

    virtual QnDbTransaction* getTransaction() = 0;

    bool applyUpdates(const QString &dirName);
    virtual bool beforeInstallUpdate(const QString& updateName);
    virtual bool afterInstallUpdate(const QString& updateName);

    static bool execSQLQuery(const QString& query, QSqlDatabase& database, const char* details);
    /*!
        \param scriptData SQL commands separated with ;
    */
    static bool execSQLScript(const QByteArray& scriptData, QSqlDatabase& database);
    //!Reads file \a fileName and calls \a QnDbHelper::execSQLScript
    static bool execSQLFile(const QString& fileName, QSqlDatabase& database);

    static bool execSQLQuery(QSqlQuery *query, const char* details);
    static bool prepareSQLQuery(QSqlQuery *query, const QString &queryStr, const char* details);

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
