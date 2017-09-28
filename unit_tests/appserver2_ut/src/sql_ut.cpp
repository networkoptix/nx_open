#include <gtest/gtest.h>
#include <thread>
#include <QtSql/QtSql>
#include <QtCore/QtCore>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>
#include <nx/utils/test_support/test_options.h>

class DataBase: public testing::Test
{
public:
    DataBase():
        m_file(nx::utils::TestOptions::temporaryDirectoryPath() + QLatin1String("/db.sqlite")),
        m_db(QSqlDatabase::addDatabase("QSQLITE", "db"))
    {
        NX_INFO(this) << "Database" << m_file;
        QDir(".").remove(m_file);

        m_db.setDatabaseName(m_file);
        NX_ASSERT(m_db.open(), m_db.lastError().text());

        execute("PRAGMA journal_mode = WAL");
        execute("PRAGMA journal_size_limit = 10000000");

        execute(R"sql(CREATE TABLE t (
            i INTEGER NOT NULL UNIQUE PRIMARY KEY,
            d VARCHAR(200) NOT NULL
        ))sql");

        logSize();
    }

    QSqlQuery query(const QString& s)
    {
        QSqlQuery q(m_db);
        NX_ASSERT(q.prepare(s), q.lastError().text(), s);
        return q;
    }

    QSqlQuery execute(const QString& s) { return execute(query(s)); }
    QSqlQuery execute(QSqlQuery q)
    {
        NX_VERBOSE(this) << q.lastQuery();
        NX_ASSERT(q.exec(), q.lastError().text());
        return q;
    }

    class Transaction
    {
    public:
        Transaction(QSqlDatabase* db): m_db(db)
        {
            NX_DEBUG(this) << "Begin";
            NX_ASSERT(m_db->transaction(), m_db->lastError().text());
        }

        ~Transaction()
        {
            NX_DEBUG(this) << "Commit";
            NX_ASSERT(m_db->commit(), m_db->lastError().text());
        }

    private:
        QSqlDatabase* m_db;
    };

    std::unique_ptr<Transaction> transaction()
    {
        return std::make_unique<Transaction>(&m_db);
    }

    QSqlQuery insert(size_t count)
    {
        QStringList records;
        for (size_t i = 0; i < count; ++i)
            records << lm("('%1')").arg(nx::utils::random::generate(10).toHex());

        NX_DEBUG(this) << lm("Insert %1 records").arg(count);
        return execute(lm("INSERT INTO t (d) VALUES %1").arg(records.join(", ")));
    }

    void insertX(size_t count)
    {
        NX_DEBUG(this) << lm("Insert %1 records by one").arg(count);
        for (size_t i = 0; i < count; ++i)
        {
            execute(lm("INSERT INTO t (d) VALUES ('%1')")
                .arg(nx::utils::random::generate(10).toHex()));
        }
    }

    void logSize() const
    {
        for (const auto& name: {m_file, m_file + "-wal"})
            NX_DEBUG(this) << name << "size:" << nx::utils::bytesToString(QFileInfo(name).size());
    }

private:
    const QString m_file;
    QSqlDatabase m_db;
};

TEST_F(DataBase, Test1)
{
    static const size_t kRecordCount = 10 * 1000;

    insert(kRecordCount);
    logSize();

    {
        const auto t = transaction();
        insertX(kRecordCount);
        logSize();
    }

    logSize();
}

TEST_F(DataBase, Test2)
{
    static const size_t kRecordCount = 10 * 1000;
    logSize();

    {
        const auto t = transaction();
        insertX(kRecordCount);
    }

    logSize();

    auto q = execute("SELECT * FROM t");
    EXPECT_TRUE(q.next()) << q.lastError().text().toStdString();
    EXPECT_EQ(1, q.value(0).toInt());
    NX_DEBUG(this) << "---- SELECT";

    for(int i = 0; i < 5; ++i)
    {
        logSize();
        const auto t = transaction();
        insertX(kRecordCount);
    }

    logSize();
    NX_DEBUG(this) << "---- FINISH";
    q.finish();

    for(int i = 0; i < 5; ++i)
    {
        logSize();
        const auto t = transaction();
        insertX(kRecordCount);
    }
}
