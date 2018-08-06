#ifndef __QN_DB_HELPER_H__
#define __QN_DB_HELPER_H__

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>

#include <nx/sql/sql_query_execution_helper.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/unused.h>
#include <nx/utils/uuid.h>
#include <nx/utils/literal.h>

class QSqlDatabase;

class QnDbHelper:
    public nx::sql::SqlQueryExecutionHelper
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
        bool dbCommit(const QString& event);
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
        nx::utils::unused(data);
#endif // DEBUG
    }

    template <class T, class Field>
    static void assertSorted(T &data, QnUuid Field::*idField)
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
        nx::utils::unused(data, idField);
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
    template <
        class MainDataVector,
        class SubDataVector,
        class MainSubDataVector,
        class MainOrParentType,
        class IdType,
        class SubOrParentType>
    static void mergeObjectListData(
        MainDataVector& data,
        SubDataVector& subDataList,
        MainSubDataVector MainOrParentType::*subDataListField,
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

    template <
        class MainDataVector,
        class SubDataVector,
        class MainOrParentType,
        class IdType,
        class SubOrParentType,
        class Handler>
    static void mergeObjectListData(
        MainDataVector& data,
        SubDataVector& subDataList,
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

    template <typename Container, typename Value>
    static void insertToContainer(Container& container, const Value& value, decltype(&Container::insert))
    {
        container.insert(value);
    }

    template <typename Container, typename Value>
    static void insertToContainer(Container& container, const Value& value, ...)
    {
        container.push_back(value);
    }

    //!Same as above but does not require field "id" of type QnUuid
    /**
    * Function merges two sorted lists. First of them (data) contains placeholder
    * field for the data, that is contained in the second (subData).
    * Data elements MUST be sorted by \a idField.
    * SubData elements should be sorted by parentIdField.
    * Types MainOrParentType1 and MainOrParentType2 are separated to allow \a subDataListField and \a idField to be pointers to fields of related types
    */
    template <
        class MainDataVector,
        class SubDataVector,
        class MainSubDataVector,
        class MainOrParentType1,
        class MainOrParentType2,
        class IdType,
        class SubOrParentType>
    static void mergeObjectListData(
        MainDataVector& data,
        SubDataVector& subDataList,
        MainSubDataVector MainOrParentType1::*subDataListField,
        IdType MainOrParentType2::*idField,
        IdType SubOrParentType::*parentIdField )
    {
        mergeObjectListData(
            data,
            subDataList,
            idField,
            parentIdField,
                [subDataListField](
                    typename MainDataVector::value_type& mergeTo,
                    typename SubDataVector::value_type& mergeWhat)
                {
                    insertToContainer(mergeTo.*subDataListField, mergeWhat, 0);
                });
    }

protected:
    bool isObjectExists(
        const QString& objectType, const QString& objectName, QSqlDatabase& database);
    void addDatabase(const QString& fileName, const QString& dbname);
    void removeDatabase();

protected:
    QSqlDatabase m_sdb;
    mutable QnReadWriteLock m_mutex;
    QString m_connectionName;
};

#endif // __QN_DB_HELPER_H__
