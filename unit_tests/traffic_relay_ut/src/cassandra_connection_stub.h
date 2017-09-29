#pragma once

#include <nx/casssandra/async_cassandra_connection.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class CassandraConnectionStub:
    public nx::cassandra::AbstractAsyncConnection
{
public:
    virtual ~CassandraConnectionStub();

    virtual void init(nx::utils::MoveOnlyFunc<void(CassError)> completionHandler) override;

    virtual void prepareQuery(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError, nx::cassandra::Query query)> completionHandler) override;

    virtual void executeSelect(
        nx::cassandra::Query query,
        nx::utils::MoveOnlyFunc<void(CassError, nx::cassandra::QueryResult result)> completionHandler) override;

    virtual void executeSelect(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError, nx::cassandra::QueryResult result)> completionHandler) override;

    virtual void executeUpdate(
        nx::cassandra::Query query,
        nx::utils::MoveOnlyFunc<void(CassError)> completionHandler) override;

    virtual void executeUpdate(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError)> completionHandler) override;

    virtual void wait() override;

    void setDbHostAvailable(bool val);

    void setInitializationDoneEventQueue(
        nx::utils::SyncQueue<bool /*initialization result*/>* queue);

private:
    bool m_isDbHostAvailable = true;
    bool m_isConnected = false;
    nx::network::aio::BasicPollable m_aioBinder;
    nx::utils::SyncQueue<bool /*initialization result*/>* m_initializationDoneEventQueue = nullptr;
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
