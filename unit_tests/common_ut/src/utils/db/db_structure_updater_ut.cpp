#include <gtest/gtest.h>

#include <list>
#include <map>
#include <string>

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/cpp14.h>

#include <nx/utils/db/db_structure_updater.h>

#include "base_db_test.h"

namespace nx {
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

class DbStructureUpdater:
    public BaseDbTest
{
    using BaseType = BaseDbTest;

public:
    DbStructureUpdater():
        m_updateResult(false)
    {
        using namespace std::placeholders;

        initializeDatabase();

        m_testAsyncSqlQueryExecutor = std::make_unique<TestAsyncSqlQueryExecutor>(
            asyncSqlQueryExecutor().get());
        m_testAsyncSqlQueryExecutor->setCustomExecSqlScriptFunc(
            std::bind(&DbStructureUpdater::execSqlScript, this, _1, _2));

        m_dbUpdater = std::make_unique<db::DbStructureUpdater>(kCdbStructureName, m_testAsyncSqlQueryExecutor.get());
    }

protected:
    void registerUpdateScriptFor(
        RdbmsDriverType dbType,
        QByteArray script = QByteArray())
    {
        if (script.isEmpty())
            script = QnLexical::serialized(dbType).toLatin1();
        m_registeredScripts.emplace(script, dbType);
        m_dbTypeToScript.emplace(dbType, script);
    }

    void registerDefaultUpdateScript(QByteArray script = QByteArray())
    {
        registerUpdateScriptFor(RdbmsDriverType::unknown, std::move(script));
    }

    void emulateConnectionToDb(RdbmsDriverType dbType)
    {
        m_testAsyncSqlQueryExecutor->connectionOptions().driverType = dbType;
    }

    void whenUpdatedDb()
    {
        m_dbUpdater->addUpdateScript(m_dbTypeToScript);
        m_updateResult = m_dbUpdater->updateStructSync();
    }

    void assertIfInvokedNotDefaultScript()
    {
        assertIfInvokedScriptForDbmsOtherThan(RdbmsDriverType::unknown);
    }

    void assertIfInvokedScriptForDbmsOtherThan(RdbmsDriverType dbType)
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

    void assertIfUpdateSucceeded()
    {
        ASSERT_FALSE(m_updateResult);
    }

    void assertIfThereWasNoInvokationOf(const QByteArray& script)
    {
        ASSERT_TRUE(
            std::find(m_executedScripts.cbegin(), m_executedScripts.cend(), script) !=
            m_executedScripts.cend());
    }

private:
    std::unique_ptr<TestAsyncSqlQueryExecutor> m_testAsyncSqlQueryExecutor;
    std::unique_ptr<nx::utils::db::DbStructureUpdater> m_dbUpdater;
    std::list<QByteArray> m_executedScripts;
    std::multimap<QByteArray, RdbmsDriverType> m_registeredScripts;
    std::map<RdbmsDriverType, QByteArray> m_dbTypeToScript;
    bool m_updateResult;

    void initializeDatabase()
    {
        BaseType::initializeDatabase();

        // Creating initial structure.
        nx::utils::db::DbStructureUpdater updater(kCdbStructureName, asyncSqlQueryExecutor().get());
        ASSERT_TRUE(updater.updateStructSync());
    }

    DBResult execSqlScript(
        const QByteArray& script,
        nx::utils::db::QueryContext* const queryContext)
    {
        m_executedScripts.push_back(script);

        if (m_registeredScripts.find(script) == m_registeredScripts.end())
            return asyncSqlQueryExecutor()->execSqlScriptSync(script, queryContext);

        return DBResult::ok;
    }
};

TEST_F(DbStructureUpdater, different_sql_scripts_for_different_dbms)
{
    registerUpdateScriptFor(RdbmsDriverType::mysql);
    registerUpdateScriptFor(RdbmsDriverType::sqlite);
    emulateConnectionToDb(RdbmsDriverType::mysql);

    whenUpdatedDb();
    assertIfInvokedScriptForDbmsOtherThan(RdbmsDriverType::mysql);
}

TEST_F(DbStructureUpdater, default_script_gets_called_if_no_dbms_specific_script_supplied)
{
    registerUpdateScriptFor(RdbmsDriverType::mysql);
    registerDefaultUpdateScript();
    emulateConnectionToDb(RdbmsDriverType::oracle);

    whenUpdatedDb();
    assertIfInvokedNotDefaultScript();
}

TEST_F(DbStructureUpdater, update_fails_if_no_suitable_script_found)
{
    registerUpdateScriptFor(RdbmsDriverType::mysql);
    registerUpdateScriptFor(RdbmsDriverType::sqlite);
    emulateConnectionToDb(RdbmsDriverType::oracle);

    whenUpdatedDb();
    assertIfUpdateSucceeded();
}

TEST_F(DbStructureUpdater, proper_dialect_fix_is_applied)
{
    const char initialScript[] = "%bigint_primary_key_auto_increment%";
    const char sqliteAdaptedScript[] = "INTEGER PRIMARY KEY AUTOINCREMENT";

    registerDefaultUpdateScript(initialScript);
    emulateConnectionToDb(RdbmsDriverType::sqlite);
    whenUpdatedDb();
    assertIfThereWasNoInvokationOf(sqliteAdaptedScript);
}

} // namespace test
} // namespace db
} // namespace nx
