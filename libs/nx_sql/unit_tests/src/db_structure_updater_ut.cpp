// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <list>
#include <map>
#include <string>

#include <nx/sql/db_connection_factory.h>
#include <nx/sql/db_structure_updater.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/algorithm.h>

#include "base_db_test.h"
#include "delegate_executor.h"

namespace nx::sql::test {

static constexpr char kCdbStructureName[] = "test";

using CustomExecSqlFunc = nx::utils::MoveOnlyFunc<DBResult(
    nx::sql::QueryContext* const queryContext,
    const std::string& script)>;

//-------------------------------------------------------------------------------------------------

class TestSqlQuery:
    public DelegateSqlQuery
{
    using base_type = DelegateSqlQuery;

public:
    using CustomExecSqlFunc = nx::utils::MoveOnlyFunc<DBResult(
        nx::sql::QueryContext*, const std::string& /*script*/)>;

    using base_type::base_type;

    virtual void prepare(const std::string_view& query) override
    {
        m_query = query;

        if (!m_customExecSqlFunc)
            base_type::prepare(query);
    }

    virtual void exec() override
    {
        if (m_customExecSqlFunc)
        {
            m_customExecSqlFunc(nullptr, m_query.c_str());
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
    TestDbConnection(const nx::sql::ConnectionOptions& dbConnectionOptions):
        base_type(nullptr),
        m_realConnection(dbConnectionOptions)
    {
        setDelegate(&m_realConnection);
    }

    virtual std::unique_ptr<AbstractSqlQuery> createQuery() override
    {
        auto query = std::make_unique<TestSqlQuery>(base_type::createQuery());
        if (m_customExecSqlFunc)
        {
            query->setCustomExecSqlFunc(
                [this](nx::sql::QueryContext*, std::string script)
                {
                    nx::sql::QueryContext queryContext(this, m_transaction.get());
                    return m_customExecSqlFunc(&queryContext, script);
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

    virtual bool begin()
    {
        // Limiting this test implementation to one transaction at a time.
        auto transaction = std::make_unique<Transaction>(&m_realConnection);
        if (transaction->begin() != nx::sql::DBResultCode::ok)
            return false;

        m_transaction = std::move(transaction);
        return true;
    }

    virtual bool commit()
    {
        if (m_onCommit)
            m_onCommit();
        return m_transaction->commit() == nx::sql::DBResultCode::ok;
    }

    virtual bool rollback()
    {
        return m_transaction->rollback() == nx::sql::DBResultCode::ok;
    }

    void setCustomExecSqlFunc(CustomExecSqlFunc func)
    {
        m_customExecSqlFunc = std::move(func);
    }

    void setOnCommit(nx::utils::MoveOnlyFunc<void()> func)
    {
        m_onCommit = std::move(func);
    }

    void setDriverType(std::optional<RdbmsDriverType> driverType)
    {
        m_driverType = driverType;
    }

private:
    CustomExecSqlFunc m_customExecSqlFunc;
    std::optional<RdbmsDriverType> m_driverType;
    std::unique_ptr<Transaction> m_transaction;
    QtDbConnection m_realConnection;
    nx::utils::MoveOnlyFunc<void()> m_onCommit;
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
    // Synchronous operations.

    virtual DBResult execSqlScript(
        nx::sql::QueryContext* const queryContext,
        const std::string& script) override
    {
        if (m_customExecSqlFunc)
            return m_customExecSqlFunc(queryContext, script);
        return base_type::execSqlScript(queryContext, script);
    }

    ConnectionOptions& connectionOptions()
    {
        return m_connectionOptions;
    }

    CustomExecSqlFunc setCustomExecSqlFunc(CustomExecSqlFunc func)
    {
        return std::exchange(m_customExecSqlFunc, std::move(func));
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
        m_dbConnectionFactoryBak = DbConnectionFactory::instance().setCustomFunc(
            [this](auto&&... args)
            {
                auto connection = std::make_unique<TestDbConnection>(
                    std::forward<decltype(args)>(args)...);
                connection->setCustomExecSqlFunc(
                    [this](nx::sql::QueryContext* queryContext, const std::string& stmt)
                    {
                        return execSqlScript(queryContext, stmt);
                    });
                connection->setOnCommit([this]() { ++m_commitCounter; });
                m_lastTestDbConnection = connection.get();

                return connection;
            });

        base_type::initializeDatabase();

        m_testAsyncSqlQueryExecutor = std::make_unique<TestAsyncSqlQueryExecutor>(
            &asyncSqlQueryExecutor());
    }

    ~BasicDbStructureUpdaterTestSetup()
    {
        DbConnectionFactory::instance().setCustomFunc(
            std::exchange(m_dbConnectionFactoryBak, nullptr));
    }

protected:
    struct ExecutedScriptContext
    {
        std::string sqlText;
        std::optional<nx::sql::DBResult> commitResult;
        int commitSeq = -1;
    };

    std::list<ExecutedScriptContext> m_executedScripts;
    std::atomic<int> m_commitCounter = 0;
    TestDbConnection* m_lastTestDbConnection = nullptr;

    TestAsyncSqlQueryExecutor& testAsyncSqlQueryExecutor()
    {
        return *m_testAsyncSqlQueryExecutor;
    }

    virtual DBResult execSqlScript(
        nx::sql::QueryContext* const queryContext,
        const std::string& script)
    {
        m_executedScripts.push_back(ExecutedScriptContext{script, std::nullopt});

        queryContext->transaction()->addOnTransactionCompletionHandler(
            [this, it = std::prev(m_executedScripts.end())](nx::sql::DBResult result)
            {
                it->commitResult = result;
                it->commitSeq = m_commitCounter;
            });

        return DBResultCode::ok;
    }

private:
    std::unique_ptr<TestAsyncSqlQueryExecutor> m_testAsyncSqlQueryExecutor;
    DbConnectionFactory::Function m_dbConnectionFactoryBak;
};

//-------------------------------------------------------------------------------------------------

class DbStructureUpdater:
    public BasicDbStructureUpdaterTestSetup
{
    using base_type = BasicDbStructureUpdaterTestSetup;

public:
    DbStructureUpdater()
    {
        initializeDatabase();

        m_dbUpdater = std::make_unique<sql::DbStructureUpdater>(
            kCdbStructureName,
            &testAsyncSqlQueryExecutor());
    }

protected:
    void registerUpdateScriptFor(
        RdbmsDriverType dbType,
        std::string script = std::string())
    {
        if (script.empty())
            script = toString(dbType);
        m_registeredScripts.emplace(script, dbType);
        m_dbTypeToScript.emplace(dbType, script);
    }

    void registerDefaultUpdateScript(std::string script = std::string())
    {
        registerUpdateScript({RdbmsDriverType::unknown}, std::move(script));
    }

    void registerUpdateScript(
        std::vector<RdbmsDriverType> dbTypes,
        std::string script = std::string())
    {
        std::map<RdbmsDriverType, std::string> dbTypeToScript;
        for (auto dbType: dbTypes)
        {
            auto typedScript = script.empty() ? toString(dbType) : script;
            dbTypeToScript.emplace(dbType, typedScript);
            m_registeredScripts.emplace(typedScript, dbType);
        }

        m_dbUpdater->addUpdateScript("script_id_" + script, dbTypeToScript);
    }

    std::vector<std::string> registerMultipleRandomUpdateScripts(int count)
    {
        std::vector<std::string> scripts;
        for (int i = 0; i < count; ++i)
        {
            auto text = nx::format("script_%1", i).toStdString();
            registerDefaultUpdateScript(text);
            scripts.push_back(text);
        }
        return scripts;
    }

    void emulateConnectionToDb(RdbmsDriverType dbType)
    {
        m_lastTestDbConnection->setDriverType(dbType);
    }

    void whenUpdateDb()
    {
        m_dbUpdater->addUpdateScript("1", m_dbTypeToScript);
        m_updateResult = m_dbUpdater->updateStructSync();
    }

    // Asserts if at least two from scripts were invoked under the same DB transaction.
    void thenEachUpdateScriptIsAppliedUnderItsOwnDbTransaction(
        const std::vector<std::string>& scripts)
    {
        std::set<int> commitSeqs;

        for (const auto& executedScript: m_executedScripts)
        {
            if (!nx::utils::contains(scripts, executedScript.sqlText))
                continue;

            ASSERT_EQ(nx::sql::DBResultCode::ok, executedScript.commitResult);
            ASSERT_TRUE(commitSeqs.insert(executedScript.commitSeq).second);
        }
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
            const auto range = m_registeredScripts.equal_range(executedScript.sqlText);
            bool scriptFitsForDbType = true;
            for (auto it = range.first; it != range.second; ++it)
            {
                if (it->second != dbType)
                {
                    scriptFitsForDbType = false;
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

    void assertThereWasInvokationOf(const std::string& script)
    {
        ASSERT_TRUE(
            std::find_if(
                m_executedScripts.cbegin(), m_executedScripts.cend(),
                [script](const auto& val) { return val.sqlText == script; }) !=
            m_executedScripts.cend());
    }

private:
    std::unique_ptr<nx::sql::DbStructureUpdater> m_dbUpdater;
    std::multimap<std::string, RdbmsDriverType> m_registeredScripts;
    std::map<RdbmsDriverType, std::string> m_dbTypeToScript;
    bool m_updateResult = false;

    void initializeDatabase()
    {
        // Creating initial structure.
        nx::sql::DbStructureUpdater updater(kCdbStructureName, &asyncSqlQueryExecutor());
        ASSERT_TRUE(updater.updateStructSync());
    }
};

TEST_F(DbStructureUpdater, different_sql_scripts_for_different_dbms)
{
    registerUpdateScript({RdbmsDriverType::mysql, RdbmsDriverType::sqlite});
    emulateConnectionToDb(RdbmsDriverType::mysql);

    whenUpdateDb();
    assertInvokedScriptForDbms(RdbmsDriverType::mysql);
}

TEST_F(DbStructureUpdater, default_script_gets_called_if_no_dbms_specific_script_supplied)
{
    registerUpdateScript({RdbmsDriverType::mysql, RdbmsDriverType::unknown});
    emulateConnectionToDb(RdbmsDriverType::oracle);

    whenUpdateDb();
    assertDefaultScriptHasBeenInvoked();
}

TEST_F(DbStructureUpdater, update_fails_if_no_suitable_script_found)
{
    registerUpdateScript({RdbmsDriverType::mysql, RdbmsDriverType::sqlite});
    emulateConnectionToDb(RdbmsDriverType::oracle);

    whenUpdateDb();
    assertUpdateFailed();
}

TEST_F(DbStructureUpdater, proper_dialect_fix_is_applied)
{
    const char initialScript[] = "%bigint_primary_key_auto_increment%";
    const char sqliteAdaptedScript[] = "INTEGER PRIMARY KEY AUTOINCREMENT";

    registerDefaultUpdateScript(initialScript);
    emulateConnectionToDb(RdbmsDriverType::sqlite);

    whenUpdateDb();

    assertThereWasInvokationOf(sqliteAdaptedScript);
}

TEST_F(DbStructureUpdater, each_update_is_applied_under_its_own_db_transaction)
{
    const auto scriptTexts = registerMultipleRandomUpdateScripts(3);
    whenUpdateDb();
    thenEachUpdateScriptIsAppliedUnderItsOwnDbTransaction(scriptTexts);
}

//-------------------------------------------------------------------------------------------------

static constexpr char kFirstSchemaName[] = "schema1";
static constexpr char kSecondSchemaName[] = "schema2";

static constexpr int kOldDbSchemaVersion = 3;

class DbStructureUpdaterMultipleSchemas:
    public BasicDbStructureUpdaterTestSetup
{
protected:
    void givenUpdatedDb()
    {
        sql::DbStructureUpdater updater(
            kFirstSchemaName,
            &testAsyncSqlQueryExecutor());
        addScripts(&updater, kFirstSchemaName, nullptr);
        ASSERT_TRUE(updater.updateStructSync());
        m_executedScripts.clear();
    }

    void givenDbBeforeMultipleSchemaSupportIntroduction()
    {
        // Enabling regular queries temporary for the purpose of creating initial DB schema.
        auto bak = testAsyncSqlQueryExecutor().setCustomExecSqlFunc(nullptr);

        testAsyncSqlQueryExecutor().executeSqlSync(R"sql(
            CREATE TABLE db_version_data (
                db_version INTEGER NOT NULL DEFAULT 0
            );
        )sql");

        testAsyncSqlQueryExecutor().executeSqlSync(
            nx::format("INSERT INTO db_version_data(db_version) VALUES (%1);")
                .arg(kOldDbSchemaVersion).toStdString());
        m_initialDbVersion = kOldDbSchemaVersion;

        testAsyncSqlQueryExecutor().setCustomExecSqlFunc(std::move(bak));
    }

    void whenLaunchedUpdaterWithAnotherSchema()
    {
        sql::DbStructureUpdater updater(
            kSecondSchemaName,
            &testAsyncSqlQueryExecutor());
        addScripts(&updater, kSecondSchemaName, &m_expectedScripts);

        // NOTE: version 1 - is an empty database.
        for (int i = 0; i < m_initialDbVersion-1; ++i)
            m_expectedScripts.pop_front();

        ASSERT_TRUE(updater.updateStructSync());
    }

    void thenScriptsOfAnotherSchemaWereExecuted()
    {
        for (const auto& expected: m_expectedScripts)
        {
            ASSERT_GT(std::count_if(m_executedScripts.begin(), m_executedScripts.end(),
                [&expected](const auto& v) { return v.sqlText == expected; }), 0);
        }
    }

private:
    std::list<std::string> m_expectedScripts;
    int m_initialDbVersion = 0;

    void addScripts(
        sql::DbStructureUpdater* updater,
        const std::string& schemaName,
        std::list<std::string>* scripts)
    {
        for (int i = 0; i < 7; ++i)
        {
            const auto script = nx::format("schema_%1_script_%2").arg(schemaName).arg(i).toStdString();
            updater->addUpdateScript(std::to_string(i), script);
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

//-------------------------------------------------------------------------------------------------

static constexpr char kSchemaName[] = "test_scheme";

class DbStructureUpdaterWithUpdateName:
    public FixtureWithQueryExecutorOnly
{
    using base_type = FixtureWithQueryExecutorOnly;

protected:
    virtual void SetUp() override
    {
        base_type::initializeDatabase();

        ASSERT_NO_THROW(asyncSqlQueryExecutor().execSqlScriptSync(R"sql(
            CREATE TABLE db_version_data (
                schema_name VARCHAR(128) NOT NULL PRIMARY KEY,
                db_version INTEGER NOT NULL DEFAULT 0
            );

            CREATE TABLE test_applied_scripts(name TEXT);
        )sql"));
    }

    void registerNewSchemaUpdates(int count = 3)
    {
        const auto sizeBak = m_updateScriptName.size();
        m_updateScriptName.resize(m_updateScriptName.size() + count);
        m_updateScript.resize(m_updateScript.size() + count);
        for (auto i = sizeBak; i < m_updateScript.size(); ++i)
        {
            m_updateScriptName[i] = nx::format("%1", i + 1).toStdString();
            m_updateScript[i] = nx::format(
                "INSERT INTO test_applied_scripts (name) VALUES (%1)", m_updateScriptName[i])
                .toStdString();
        }
    }

    void givenEmptyDb()
    {
    }

    void givenSchemaWithNumericVersion(int version)
    {
        givenSchemaWithNumericVersion(kSchemaName, version);
    }

    void givenSchemaWithNumericVersion(const std::string& schemaName, int version)
    {
        ASSERT_NO_THROW(asyncSqlQueryExecutor().execSqlScriptSync(nx::format(R"sql(
            INSERT INTO db_version_data (schema_name, db_version) VALUES("%1", %2);
            )sql", schemaName, version).toStdString()));
    }

    void givenDbWithSomeSchema()
    {
        // Some scripts are registered already.
        whenRunUpdater();
        thenAllScriptsAreApplied();
    }

    void whenRunUpdater()
    {
        whenRunUpdater(kSchemaName);
    }

    void whenRunUpdater(const std::string& schemaName)
    {
        std::vector<int> idx(m_updateScriptName.size(), 0);
        std::iota(idx.begin(), idx.end(), 0);
        whenRunUpdater(schemaName, idx);
    }

    template<typename Indices>
    void whenRunUpdater(const std::string& schemaName, const Indices& indices)
    {
        m_dbUpdater = std::make_unique<sql::DbStructureUpdater>(
            schemaName,
            &asyncSqlQueryExecutor());

        for (const auto i: indices)
        {
            m_dbUpdater->addUpdateScript(m_updateScriptName[i], m_updateScript[i]);

            if (!nx::utils::contains(m_scriptsRegisteredWithUpdater, m_updateScriptName[i]))
                m_scriptsRegisteredWithUpdater.push_back(m_updateScriptName[i]);
        }

        ASSERT_NO_THROW(m_dbUpdater->updateStructSyncThrow());
    }

    void whenRunUpdaterWithRandomScripts(int count)
    {
        std::set<int> scriptsToApply;
        for (int i = 0; i < count; ++i)
            scriptsToApply.insert(rand() % m_updateScript.size());

        whenRunUpdater(kSchemaName, scriptsToApply);
    }

    void thenAllScriptsAreApplied()
    {
        assertAppliedScriptsEqual(m_scriptsRegisteredWithUpdater);
    }

    void thenNoScriptsAreAppliedToDb()
    {
        assertAppliedScriptsEqual({});
    }

    void thenSomeScriptsAreApplied(std::vector<std::string> expected)
    {
        assertAppliedScriptsEqual(expected);
    }

    void andAllScriptsAreMarkedAsApplied()
    {
        andAllScriptsAreMarkedAsApplied(kSchemaName);
    }

    void andAllScriptsAreMarkedAsApplied(const std::string& schemaName)
    {
        assertMarkedAsAppliedScriptsEqual(schemaName, m_updateScriptName);
    }

private:
    void assertAppliedScriptsEqual(const std::vector<std::string>& expected)
    {
        assertMarkedAsAppliedScriptsEqualInternal(
            "SELECT name FROM test_applied_scripts ORDER BY rowid ASC",
            expected);
    }

    void assertMarkedAsAppliedScriptsEqual(
        const std::string& schemaName,
        const std::vector<std::string>& expected)
    {
        assertMarkedAsAppliedScriptsEqualInternal(
            nx::format("SELECT script_name "
                "FROM db_schema_applied_scripts "
                "WHERE schema_name='%1' "
                "ORDER BY rowid", schemaName).toStdString(),
            expected);
    }

    void assertMarkedAsAppliedScriptsEqualInternal(
        const std::string& querySql,
        const std::vector<std::string>& expected)
    {
        auto appliedScripts = asyncSqlQueryExecutor().executeSelectSync(
            [&querySql](nx::sql::QueryContext* queryContext)
            {
                auto query = queryContext->connection()->createQuery();
                query->prepare(querySql);
                query->exec();
                std::vector<std::string> scripts;
                while (query->next())
                    scripts.push_back(query->value(0).toString().toStdString());
                return scripts;
            });

        ASSERT_EQ(expected, appliedScripts);
    }

private:
    std::vector<std::string> m_updateScriptName;
    std::vector<std::string> m_updateScript;
    std::vector<std::string> m_scriptsRegisteredWithUpdater;
    std::unique_ptr<sql::DbStructureUpdater> m_dbUpdater;
};

TEST_F(
    DbStructureUpdaterWithUpdateName,
    migration_from_numeric_versions_to_text_label_fills_applied_scripts_properly)
{
    registerNewSchemaUpdates(3);
    // NOTE: numeric version was set to the number of the next script to apply.
    givenSchemaWithNumericVersion(3 + 1);

    whenRunUpdater();

    thenNoScriptsAreAppliedToDb();
    andAllScriptsAreMarkedAsApplied();
}

TEST_F(
    DbStructureUpdaterWithUpdateName,
    multiple_schemes_with_numeric_versions_are_migrated_properly)
{
    constexpr int count = 2;

    registerNewSchemaUpdates(3);

    // Creating multiple schemes in the DB.
    for (int i = 0; i < count; ++i)
        givenSchemaWithNumericVersion(nx::format("schema_%1", i).toStdString(), 3 + 1);

    // Running schema updaters on all schemes.
    for (int i = 0; i < count; ++i)
    {
        const auto schemaName = nx::format("schema_%1", i).toStdString();

        whenRunUpdater(schemaName);

        thenNoScriptsAreAppliedToDb();
        andAllScriptsAreMarkedAsApplied(schemaName);
    }
}

TEST_F(
    DbStructureUpdaterWithUpdateName,
    new_scripts_are_applied_just_after_migration)
{
    registerNewSchemaUpdates(5);
    givenSchemaWithNumericVersion(3 + 1);

    whenRunUpdater();

    thenSomeScriptsAreApplied({"4", "5"});
    andAllScriptsAreMarkedAsApplied();
}

TEST_F(DbStructureUpdaterWithUpdateName, on_empty_db_all_scripts_are_applied)
{
    registerNewSchemaUpdates(3);
    givenEmptyDb();

    whenRunUpdater();

    thenAllScriptsAreApplied();
}

TEST_F(DbStructureUpdaterWithUpdateName, non_applied_scripts_are_applied)
{
    registerNewSchemaUpdates(3);
    givenDbWithSomeSchema();

    registerNewSchemaUpdates(3);
    whenRunUpdater();

    thenAllScriptsAreApplied();
}

TEST_F(DbStructureUpdaterWithUpdateName, non_applied_scripts_are_applied_in_registration_order)
{
    registerNewSchemaUpdates(11);

    whenRunUpdaterWithRandomScripts(5);
    // Only 5 update scripts were applied to the DB.

    whenRunUpdater();
    // All other scripts are applied after the first 5, but in initial relative order.

    thenAllScriptsAreApplied();
    // Order is validated.
}

} // namespace nx::sql::test
