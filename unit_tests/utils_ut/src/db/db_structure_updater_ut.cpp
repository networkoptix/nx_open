#include <gtest/gtest.h>

#include <list>
#include <map>
#include <string>

#include <nx/utils/db/db_structure_updater.h>
#include <nx/utils/std/cpp14.h>

#include "base_db_test.h"

namespace nx {
namespace utils {
namespace db {
namespace test {

static const std::string kCdbStructureName = "test";

class TestAsyncSqlQueryExecutor:
    public AbstractAsyncSqlQueryExecutor
{
public:
    typedef nx::utils::MoveOnlyFunc<DBResult(
        const QByteArray& script,
        nx::utils::db::QueryContext* const queryContext)> CustomExecSqlScriptFunc;

    TestAsyncSqlQueryExecutor(AbstractAsyncSqlQueryExecutor* _delegate):
        m_delegate(_delegate)
    {
        m_connectionOptions = m_delegate->connectionOptions();
    }

    virtual const ConnectionOptions& connectionOptions() const override
    {
        return m_connectionOptions;
    }

    //---------------------------------------------------------------------------------------------
    // Asynchronous operations.

    virtual void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(nx::utils::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, DBResult)> completionHandler) override
    {
        m_delegate->executeUpdate(
            std::move(dbUpdateFunc), std::move(completionHandler));
    }

    virtual void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<DBResult(nx::utils::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, DBResult)> completionHandler) override
    {
        m_delegate->executeUpdateWithoutTran(
            std::move(dbUpdateFunc), std::move(completionHandler));
    }

    virtual void executeSelect(
        nx::utils::MoveOnlyFunc<DBResult(nx::utils::db::QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, DBResult)> completionHandler) override
    {
        m_delegate->executeSelect(std::move(dbSelectFunc), std::move(completionHandler));
    }

    //---------------------------------------------------------------------------------------------
    // Synchronous operations.

    virtual DBResult execSqlScriptSync(
        const QByteArray& script,
        nx::utils::db::QueryContext* const queryContext) override
    {
        if (m_customExecSqlScriptFunc)
            return m_customExecSqlScriptFunc(script, queryContext);
        return m_delegate->execSqlScriptSync(script, queryContext);
    }

    ConnectionOptions& connectionOptions()
    {
        return m_connectionOptions;
    }

    void setCustomExecSqlScriptFunc(CustomExecSqlScriptFunc func)
    {
        m_customExecSqlScriptFunc = std::move(func);
    }

private:
    ConnectionOptions m_connectionOptions;
    AbstractAsyncSqlQueryExecutor* m_delegate;
    CustomExecSqlScriptFunc m_customExecSqlScriptFunc;
};

//-------------------------------------------------------------------------------------------------

class BasicDbStructureUpdaterTestSetup:
    public BaseDbTest
{
    using base_type = BaseDbTest;

public:
    BasicDbStructureUpdaterTestSetup()
    {
        using namespace std::placeholders;

        base_type::initializeDatabase();

        m_testAsyncSqlQueryExecutor = std::make_unique<TestAsyncSqlQueryExecutor>(
            &asyncSqlQueryExecutor());
        m_testAsyncSqlQueryExecutor->setCustomExecSqlScriptFunc(
            std::bind(&BasicDbStructureUpdaterTestSetup::execSqlScript, this, _1, _2));
    }

protected:
    std::list<QByteArray> m_executedScripts;

    TestAsyncSqlQueryExecutor& testAsyncSqlQueryExecutor()
    {
        return *m_testAsyncSqlQueryExecutor;
    }

    virtual DBResult execSqlScript(
        const QByteArray& script,
        nx::utils::db::QueryContext* const /*queryContext*/)
    {
        m_executedScripts.push_back(script);
        return DBResult::ok;
    }

private:
    std::unique_ptr<TestAsyncSqlQueryExecutor> m_testAsyncSqlQueryExecutor;
};

//-------------------------------------------------------------------------------------------------

class DbStructureUpdater:
    public BasicDbStructureUpdaterTestSetup
{
    using base_type = BasicDbStructureUpdaterTestSetup;

public:
    DbStructureUpdater():
        m_updateResult(false)
    {
        initializeDatabase();

        m_dbUpdater = std::make_unique<db::DbStructureUpdater>(
            kCdbStructureName,
            &testAsyncSqlQueryExecutor());
    }

protected:
    void registerUpdateScriptFor(
        RdbmsDriverType dbType,
        QByteArray script = QByteArray())
    {
        if (script.isEmpty())
            script = toString(dbType);
        m_registeredScripts.emplace(script, dbType);
        m_dbTypeToScript.emplace(dbType, script);
    }

    void registerDefaultUpdateScript(QByteArray script = QByteArray())
    {
        registerUpdateScriptFor(RdbmsDriverType::unknown, std::move(script));
    }

    void emulateConnectionToDb(RdbmsDriverType dbType)
    {
        testAsyncSqlQueryExecutor().connectionOptions().driverType = dbType;
    }

    void whenUpdatedDb()
    {
        m_dbUpdater->addUpdateScript(m_dbTypeToScript);
        m_updateResult = m_dbUpdater->updateStructSync();
    }

    void assertDefaultScriptHasBeenInvoked()
    {
        assertInvokedScriptForDbms(RdbmsDriverType::unknown);
    }

    void assertInvokedScriptForDbms(RdbmsDriverType dbType)
    {
        ASSERT_TRUE(m_updateResult);

        for (const auto& executedScript: m_executedScripts)
        {
            const auto range = m_registeredScripts.equal_range(executedScript);
            bool scriptFitsForDbType = false;
            for (auto it = range.first; it != range.second; ++it)
            {
                if (it->second == dbType)
                {
                    scriptFitsForDbType = true;
                    break;
                }
            }

            ASSERT_TRUE(scriptFitsForDbType);
        }
    }

    void assertUpdateFailed()
    {
        ASSERT_FALSE(m_updateResult);
    }

    void assertThereWasInvokationOf(const QByteArray& script)
    {
        ASSERT_TRUE(
            std::find(m_executedScripts.cbegin(), m_executedScripts.cend(), script) !=
            m_executedScripts.cend());
    }

private:
    std::unique_ptr<nx::utils::db::DbStructureUpdater> m_dbUpdater;
    std::multimap<QByteArray, RdbmsDriverType> m_registeredScripts;
    std::map<RdbmsDriverType, QByteArray> m_dbTypeToScript;
    bool m_updateResult;

    void initializeDatabase()
    {
        // Creating initial structure.
        nx::utils::db::DbStructureUpdater updater(kCdbStructureName, &asyncSqlQueryExecutor());
        ASSERT_TRUE(updater.updateStructSync());
    }

    virtual DBResult execSqlScript(
        const QByteArray& script,
        nx::utils::db::QueryContext* const queryContext) override
    {
        auto dbResult = base_type::execSqlScript(script, queryContext);
        if (dbResult != DBResult::ok)
            return dbResult;

        if (m_registeredScripts.find(script) == m_registeredScripts.end())
            return asyncSqlQueryExecutor().execSqlScriptSync(script, queryContext);

        return DBResult::ok;
    }
};

TEST_F(DbStructureUpdater, different_sql_scripts_for_different_dbms)
{
    registerUpdateScriptFor(RdbmsDriverType::mysql);
    registerUpdateScriptFor(RdbmsDriverType::sqlite);
    emulateConnectionToDb(RdbmsDriverType::mysql);

    whenUpdatedDb();
    assertInvokedScriptForDbms(RdbmsDriverType::mysql);
}

TEST_F(DbStructureUpdater, default_script_gets_called_if_no_dbms_specific_script_supplied)
{
    registerUpdateScriptFor(RdbmsDriverType::mysql);
    registerDefaultUpdateScript();
    emulateConnectionToDb(RdbmsDriverType::oracle);

    whenUpdatedDb();
    assertDefaultScriptHasBeenInvoked();
}

TEST_F(DbStructureUpdater, update_fails_if_no_suitable_script_found)
{
    registerUpdateScriptFor(RdbmsDriverType::mysql);
    registerUpdateScriptFor(RdbmsDriverType::sqlite);
    emulateConnectionToDb(RdbmsDriverType::oracle);

    whenUpdatedDb();
    assertUpdateFailed();
}

TEST_F(DbStructureUpdater, proper_dialect_fix_is_applied)
{
    const char initialScript[] = "%bigint_primary_key_auto_increment%";
    const char sqliteAdaptedScript[] = "INTEGER PRIMARY KEY AUTOINCREMENT";

    registerDefaultUpdateScript(initialScript);
    emulateConnectionToDb(RdbmsDriverType::sqlite);
    whenUpdatedDb();
    assertThereWasInvokationOf(sqliteAdaptedScript);
}

//-------------------------------------------------------------------------------------------------

static const char* firstSchemaName = "schema1";
static const char* secondSchemaName = "schema2";

static constexpr int kOldDbSchemaVersion = 3;

class DbStructureUpdaterMultipleSchemas:
    public BasicDbStructureUpdaterTestSetup
{
protected:
    void givenUpdatedDb()
    {
        db::DbStructureUpdater updater(
            firstSchemaName,
            &testAsyncSqlQueryExecutor());
        addScripts(&updater, firstSchemaName, nullptr);
        ASSERT_TRUE(updater.updateStructSync());
        m_executedScripts.clear();
    }

    void givenDbBeforeMultipleSchemaSupportIntroduction()
    {
        testAsyncSqlQueryExecutor().executeSqlSync(R"sql(
            CREATE TABLE db_version_data (
                db_version INTEGER NOT NULL DEFAULT 0
            );
        )sql");

        testAsyncSqlQueryExecutor().executeSqlSync(
            lm("INSERT INTO db_version_data(db_version) VALUES (%1);")
                .arg(kOldDbSchemaVersion).toUtf8());
        m_initialDbVersion = kOldDbSchemaVersion;
    }

    void whenLaunchedUpdaterWithAnotherSchema()
    {
        db::DbStructureUpdater updater(
            secondSchemaName,
            &testAsyncSqlQueryExecutor());
        addScripts(&updater, secondSchemaName, &m_expectedScripts);

        // NOTE: version 1 - is an empty database.
        for (int i = 0; i < m_initialDbVersion-1; ++i)
            m_expectedScripts.pop_front();

        ASSERT_TRUE(updater.updateStructSync());
    }

    void thenScriptsOfAnotherSchemaWereExecuted()
    {
        ASSERT_EQ(m_expectedScripts, m_executedScripts);
    }

private:
    std::list<QByteArray> m_expectedScripts;
    int m_initialDbVersion = 0;

    void addScripts(
        db::DbStructureUpdater* updater,
        const std::string& schemaName,
        std::list<QByteArray>* scripts)
    {
        for (int i = 0; i < 7; ++i)
        {
            const auto script = lm("schema_%1_script_%2").arg(schemaName).arg(i).toUtf8();
            updater->addUpdateScript(script);
            if (scripts)
                scripts->push_back(script);
        }
    }
};

TEST_F(DbStructureUpdaterMultipleSchemas, second_schema_is_applied)
{
    givenUpdatedDb();
    whenLaunchedUpdaterWithAnotherSchema();
    thenScriptsOfAnotherSchemaWereExecuted();
}

TEST_F(
    DbStructureUpdaterMultipleSchemas,
    updating_db_created_before_introducing_multiple_schema_support)
{
    givenDbBeforeMultipleSchemaSupportIntroduction();
    whenLaunchedUpdaterWithAnotherSchema();
    thenScriptsOfAnotherSchemaWereExecuted();
}

} // namespace test
} // namespace db
} // namespace utils
} // namespace nx
