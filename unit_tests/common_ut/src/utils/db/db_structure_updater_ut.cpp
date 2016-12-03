#include <gtest/gtest.h>

#include <list>
#include <map>

#include <nx/fusion/model_functions.h>
#include <nx/utils/std/cpp14.h>

#include <utils/db/db_structure_updater.h>

#include "base_db_test.h"

namespace nx {
namespace db {
namespace test {

class TestAsyncSqlQueryExecutor:
    public AbstractAsyncSqlQueryExecutor
{
public:
    typedef nx::utils::MoveOnlyFunc<DBResult(
        const QByteArray& script,
        nx::db::QueryContext* const queryContext)> CustomExecSqlScriptFunc;

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
        nx::utils::MoveOnlyFunc<DBResult(nx::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::db::QueryContext*, DBResult)> completionHandler) override
    {
        m_delegate->executeUpdate(
            std::move(dbUpdateFunc), std::move(completionHandler));
    }

    virtual void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<DBResult(nx::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::db::QueryContext*, DBResult)> completionHandler) override
    {
        m_delegate->executeUpdateWithoutTran(
            std::move(dbUpdateFunc), std::move(completionHandler));
    }

    //---------------------------------------------------------------------------------------------
    // Synchronous operations.

    virtual DBResult execSqlScriptSync(
        const QByteArray& script,
        nx::db::QueryContext* const queryContext) override
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

class DBStructureUpdater:
    public BaseDbTest
{
public:
    DBStructureUpdater()
    {
        using namespace std::placeholders;

        initializeDatabase();

        m_testAsyncSqlQueryExecutor = std::make_unique<TestAsyncSqlQueryExecutor>(
            asyncSqlQueryExecutor().get());
        m_testAsyncSqlQueryExecutor->setCustomExecSqlScriptFunc(
            std::bind(&DBStructureUpdater::execSqlScript, this, _1, _2));

        m_dbUpdater = std::make_unique<db::DBStructureUpdater>(m_testAsyncSqlQueryExecutor.get());
    }

protected:
    void registerUpdateScriptFor(RdbmsDriverType dbType)
    {
        const auto script = QnLexical::serialized(dbType).toLatin1();
        m_dbUpdater->addUpdateScript(/*dbType,*/ script);
    }

    void emulateConnectionToDb(RdbmsDriverType dbType)
    {
        m_testAsyncSqlQueryExecutor->connectionOptions().driverType = dbType;
    }

    void whenUpdatedDb()
    {
        ASSERT_TRUE(m_dbUpdater->updateStructSync());
    }

    void assertIfInvokedScriptForDbmsOtherThan(RdbmsDriverType dbType)
    {
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

private:
    std::unique_ptr<TestAsyncSqlQueryExecutor> m_testAsyncSqlQueryExecutor;
    std::unique_ptr<db::DBStructureUpdater> m_dbUpdater;
    std::list<QByteArray> m_executedScripts;
    std::multimap<QByteArray, RdbmsDriverType> m_registeredScripts;

    DBResult execSqlScript(
        const QByteArray& script,
        nx::db::QueryContext* const queryContext)
    {
        if (m_registeredScripts.find(script) == m_registeredScripts.end())
            return asyncSqlQueryExecutor()->execSqlScriptSync(script, queryContext);

        m_executedScripts.push_back(script);
        return DBResult::ok;
    }
};

TEST_F(DBStructureUpdater, different_sql_scripts_for_different_dbms)
{
    registerUpdateScriptFor(RdbmsDriverType::mysql);
    registerUpdateScriptFor(RdbmsDriverType::sqlite);
    emulateConnectionToDb(RdbmsDriverType::mysql);

    whenUpdatedDb();
    assertIfInvokedScriptForDbmsOtherThan(RdbmsDriverType::mysql);
}

} // namespace test
} // namespace db
} // namespace nx
