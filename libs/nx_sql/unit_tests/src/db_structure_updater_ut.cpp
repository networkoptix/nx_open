// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <list>
#include <map>
#include <string>

#include <nx/sql/db_structure_updater.h>
#include <nx/utils/std/cpp14.h>

#include "base_db_test.h"
#include "delegate_executor.h"

namespace nx::sql::test {

static constexpr char kCdbStructureName[] = "test";

using CustomExecSqlFunc = nx::utils::MoveOnlyFunc<DBResult(
    const QByteArray& script,
    nx::sql::QueryContext* const queryContext)>;

//-------------------------------------------------------------------------------------------------

class TestSqlQuery:
    public DelegateSqlQuery
{
    using base_type = DelegateSqlQuery;

public:
    using base_type::base_type;

    virtual void prepare(const std::string_view& query) override
    {
        m_query = query;

        if (!m_customExecSqlFunc)
            return base_type::prepare(query);
    }

    virtual void exec() override
    {
        if (m_customExecSqlFunc)
        {
            m_customExecSqlFunc(m_query.c_str(), nullptr);
            return;
        }

        return base_type::exec();
    }

    void setCustomExecSqlFunc(CustomExecSqlFunc func)
    {
        m_customExecSqlFunc = std::move(func);
    }

private:
    std::string m_query;
    CustomExecSqlFunc m_customExecSqlFunc;
};

//-------------------------------------------------------------------------------------------------

class TestDbConnection:
    public DelegateDbConnection
{
    using base_type = DelegateDbConnection;

public:
    using base_type::base_type;

    virtual std::unique_ptr<AbstractSqlQuery> createQuery() override
    {
        auto query = std::make_unique<TestSqlQuery>(base_type::createQuery());
        if (m_customExecSqlFunc)
        {
            query->setCustomExecSqlFunc(
                [this](auto&&... args)
                {
                    return m_customExecSqlFunc(std::forward<decltype(args)>(args)...);
                });
        }

        return query;
    }

    virtual RdbmsDriverType driverType() const override
    {
        if (m_driverType)
            return *m_driverType;
        return base_type::driverType();
    }

    void setCustomExecSqlFunc(CustomExecSqlFunc func)
    {
        m_customExecSqlFunc = std::move(func);
    }

    void setDriverType(std::optional<RdbmsDriverType> driverType)
    {
        m_driverType = driverType;
    }

private:
    CustomExecSqlFunc m_customExecSqlFunc;
    std::optional<RdbmsDriverType> m_driverType;
};

//-------------------------------------------------------------------------------------------------

class TestAsyncSqlQueryExecutor:
    public DelegateAsyncSqlQueryExecutor
{
    using base_type = DelegateAsyncSqlQueryExecutor;

public:
    using base_type::base_type;

    TestAsyncSqlQueryExecutor(AbstractAsyncSqlQueryExecutor* _delegate):
        base_type(_delegate)
    {
        m_connectionOptions = base_type::connectionOptions();
    }

    virtual const ConnectionOptions& connectionOptions() const override
    {
        return m_connectionOptions;
    }

    //---------------------------------------------------------------------------------------------
    // Asynchronous operations.

    virtual void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
        const std::string& queryAggregationKey) override
    {
        base_type::executeUpdate(
            [this, dbUpdateFunc = std::move(dbUpdateFunc)](
                nx::sql::QueryContext* queryContext)
            {
                auto connection =
                    std::make_unique<TestDbConnection>(queryContext->connection());
                if (m_customExecSqlFunc)
                {
                    connection->setCustomExecSqlFunc(
                        [this](const QByteArray& stmt, nx::sql::QueryContext* queryContext)
                        {
                            return m_customExecSqlFunc(stmt, queryContext);
                        });
                }
                connection->setDriverType(m_connectionOptions.driverType);

                nx::sql::QueryContext replaced(
                    connection.get(),
                    queryContext->transaction());
                return dbUpdateFunc(&replaced);
            },
            std::move(completionHandler),
            queryAggregationKey);
    }

    //---------------------------------------------------------------------------------------------
    // Synchronous operations.

    virtual DBResult execSqlScriptSync(
        const QByteArray& script,
        nx::sql::QueryContext* const queryContext) override
    {
        if (m_customExecSqlFunc)
            return m_customExecSqlFunc(script, queryContext);
        return base_type::execSqlScriptSync(script, queryContext);
    }

    ConnectionOptions& connectionOptions()
    {
        return m_connectionOptions;
    }

    void setCustomExecSqlFunc(CustomExecSqlFunc func)
    {
        m_customExecSqlFunc = std::move(func);
    }

private:
    ConnectionOptions m_connectionOptions;
    CustomExecSqlFunc m_customExecSqlFunc;
};

//-------------------------------------------------------------------------------------------------

class BasicDbStructureUpdaterTestSetup:
    public FixtureWithQueryExecutorOnly
{
    using base_type = FixtureWithQueryExecutorOnly;

public:
    BasicDbStructureUpdaterTestSetup()
    {
        using namespace std::placeholders;

        base_type::initializeDatabase();

        m_testAsyncSqlQueryExecutor = std::make_unique<TestAsyncSqlQueryExecutor>(
            &asyncSqlQueryExecutor());
        m_testAsyncSqlQueryExecutor->setCustomExecSqlFunc(
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
        nx::sql::QueryContext* const /*queryContext*/)
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

        m_dbUpdater = std::make_unique<sql::DbStructureUpdater>(
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
    std::unique_ptr<nx::sql::DbStructureUpdater> m_dbUpdater;
    std::multimap<QByteArray, RdbmsDriverType> m_registeredScripts;
    std::map<RdbmsDriverType, QByteArray> m_dbTypeToScript;
    bool m_updateResult;

    void initializeDatabase()
    {
        // Creating initial structure.
        nx::sql::DbStructureUpdater updater(kCdbStructureName, &asyncSqlQueryExecutor());
        ASSERT_TRUE(updater.updateStructSync());
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
        sql::DbStructureUpdater updater(
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
            nx::format("INSERT INTO db_version_data(db_version) VALUES (%1);")
                .arg(kOldDbSchemaVersion).toUtf8());
        m_initialDbVersion = kOldDbSchemaVersion;
    }

    void whenLaunchedUpdaterWithAnotherSchema()
    {
        sql::DbStructureUpdater updater(
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
        sql::DbStructureUpdater* updater,
        const std::string& schemaName,
        std::list<QByteArray>* scripts)
    {
        for (int i = 0; i < 7; ++i)
        {
            const auto script = nx::format("schema_%1_script_%2").arg(schemaName).arg(i).toUtf8();
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

} // namespace nx::sql::test
