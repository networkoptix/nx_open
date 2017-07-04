#ifndef __QN_DB_HELPER_H__
#define __QN_DB_HELPER_H__

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>

#include <nx/utils/db/sql_query_execution_helper.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

class QSqlDatabase;

class QnDbHelper:
    public nx::utils::db::SqlQueryExecutionHelper
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

    class QnAbstractTransactionLocker
    {
    public:
        virtual ~QnAbstractTransactionLocker() {}
        virtual bool commit() = 0;
    };

    class QnDbTransactionLocker: public QnAbstractTransactionLocker
    {
    public:
        QnDbTransactionLocker(QnDbTransaction* tran);
        virtual ~QnDbTransactionLocker();
        virtual bool commit() override;

    private:
        bool m_committed;
        QnDbTransaction* m_tran;
    };

    QnDbHelper();
    virtual ~QnDbHelper();

    virtual bool tuneDBAfterOpen(QSqlDatabase* const sqlDb);

    virtual QnDbTransaction* getTransaction() = 0;

    bool applyUpdates(const QString &dirName);
    virtual bool beforeInstallUpdate(const QString& updateName);
    virtual bool afterInstallUpdate(const QString& updateName);

    template <class T>
    static void assertSorted(std::vector<T> &data)
    {
#ifdef _DEBUG
        assertSorted(data, &T::id);
#else
        Q_UNUSED(data);
#endif // DEBUG
    }

    template <class T, class Field>
    static void assertSorted(std::vector<T> &data, QnUuid Field::*idField)
    {
#ifdef _DEBUG
        if (data.empty())
            return;

        QByteArray prev = (data[0].*idField).toRfc4122();
        for (size_t i = 1; i < data.size(); ++i)
        {
            QByteArray next = (data[i].*idField).toRfc4122();
            NX_ASSERT(next >= prev);
            prev = next;
        }
#else
        Q_UNUSED(data);
        Q_UNUSED(idField);
#endif // DEBUG
    }

    /**
    * Function merges sorted query into the sorted Data list. Data list contains placeholder
    * field for the data, that is contained in the query.
    * Data elements should have 'id' field and should be sorted by it.
    * Query should have 'id' and 'parentId' fields and should be sorted by 'id'.
    */
    template <class MainData>
    static void mergeIdListData(QSqlQuery& query, std::vector<MainData>& data, std::vector<QnUuid> MainData::*subList)
    {
        assertSorted(data);

        QSqlRecord rec = query.record();
        int idIdx = rec.indexOf(lit("id"));
        int parentIdIdx = rec.indexOf(lit("parentId"));
        NX_ASSERT(idIdx >=0 && parentIdIdx >= 0);

        bool eof = true;
        QnUuid id, parentId;
        QByteArray idRfc;

        auto step = [&eof, &id, &idRfc, &parentId, &query, idIdx, parentIdIdx]
        {
            eof = !query.next();
            if (eof)
                return;
            idRfc = query.value(idIdx).toByteArray();
            NX_ASSERT(idRfc == id.toRfc4122() || idRfc > id.toRfc4122());
            id = QnUuid::fromRfc4122(idRfc);
            parentId = QnUuid::fromRfc4122(query.value(parentIdIdx).toByteArray());
        };

        step();
        size_t i = 0;
        while (i < data.size() && !eof)
        {
            if (id == data[i].id)
            {
                (data[i].*subList).push_back(parentId);
                step();
            }
            else if (idRfc > data[i].id.toRfc4122())
            {
                ++i;
            }
            else
            {
                step();
            }
        }
    }

    /**
    * Function merges two sorted lists. First of them (data) contains placeholder
    * field for the data, that is contained in the second (subData).
    * Data elements should have 'id' field and should be sorted by it.
    * SubData elements should be sorted by parentIdField.
    */
    template <class MainData, class SubData, class MainSubData, class MainOrParentType, class IdType, class SubOrParentType>
    static void mergeObjectListData(
        std::vector<MainData>& data,
        std::vector<SubData>& subDataList,
        std::vector<MainSubData> MainOrParentType::*subDataListField,
        IdType SubOrParentType::*parentIdField )
    {
        assertSorted(data);
        assertSorted(subDataList, parentIdField);

        size_t i = 0;
        size_t j = 0;
        while (i < data.size() && j < subDataList.size())
        {
            if (subDataList[j].*parentIdField == data[i].id)
            {
                (data[i].*subDataListField).push_back(subDataList[j]);
                ++j;
            }
            else if ((subDataList[j].*parentIdField).toRfc4122() > data[i].id.toRfc4122())
            {
                ++i;
            }
            else
            {
                ++j;
            }
        }
    }

    template <class MainData, class SubData, class MainOrParentType, class IdType, class SubOrParentType, class Handler>
    static void mergeObjectListData(
        std::vector<MainData>& data,
        std::vector<SubData>& subDataList,
        IdType MainOrParentType::*idField,
        IdType SubOrParentType::*parentIdField,
        Handler mergeHandler )
    {
        assertSorted(data, idField);
        assertSorted(subDataList, parentIdField);

        size_t i = 0;
        size_t j = 0;

        while (i < data.size() && j < subDataList.size())
        {
            if (subDataList[j].*parentIdField == data[i].*idField)
            {
                mergeHandler( data[i], subDataList[j] );
                ++j;
            }
            else if ((subDataList[j].*parentIdField).toRfc4122() > (data[i].*idField).toRfc4122())
            {
                ++i;
            }
            else
            {
                ++j;
            }
        }
    }

    //!Same as above but does not require field "id" of type QnUuid
    /**
    * Function merges two sorted lists. First of them (data) contains placeholder
    * field for the data, that is contained in the second (subData).
    * Data elements MUST be sorted by \a idField.
    * SubData elements should be sorted by parentIdField.
    * Types MainOrParentType1 and MainOrParentType2 are separated to allow \a subDataListField and \a idField to be pointers to fields of related types
    */
    template <class MainData, class SubData, class MainSubData, class MainOrParentType1, class MainOrParentType2, class IdType, class SubOrParentType>
    static void mergeObjectListData(
        std::vector<MainData>& data,
        std::vector<SubData>& subDataList,
        std::vector<MainSubData> MainOrParentType1::*subDataListField,
        IdType MainOrParentType2::*idField,
        IdType SubOrParentType::*parentIdField )
    {
        mergeObjectListData(
            data,
            subDataList,
            idField,
            parentIdField,
            [subDataListField]( MainData& mergeTo, SubData& mergeWhat ){ (mergeTo.*subDataListField).push_back(mergeWhat); } );
    }

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
