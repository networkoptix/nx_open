#include "cassandra_connection_stub.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

CassandraConnectionStub::~CassandraConnectionStub()
{
    m_aioBinder.pleaseStopSync();
}

void CassandraConnectionStub::init(nx::utils::MoveOnlyFunc<void(CassError)> completionHandler)
{
    m_aioBinder.post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            m_isConnected = m_isDbHostAvailable;
            if (m_initializationDoneEventQueue)
                m_initializationDoneEventQueue->push(m_isConnected);
            completionHandler(
                m_isDbHostAvailable
                    ? CassError::CASS_OK
                    : CassError::CASS_ERROR_LIB_NOT_IMPLEMENTED);
        });
}

void CassandraConnectionStub::prepareQuery(
    const char* /*queryString*/,
    nx::utils::MoveOnlyFunc<void(CassError, nx::cassandra::Query query)> completionHandler)
{
    m_aioBinder.post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            completionHandler(
                CassError::CASS_ERROR_LIB_NOT_IMPLEMENTED,
                nx::cassandra::Query());
        });
}

void CassandraConnectionStub::executeSelect(
    nx::cassandra::Query /*query*/,
    nx::utils::MoveOnlyFunc<void(CassError, nx::cassandra::QueryResult result)> completionHandler)
{
    m_aioBinder.post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            completionHandler(
                CassError::CASS_ERROR_LIB_NOT_IMPLEMENTED,
                nx::cassandra::QueryResult());
        });
}

void CassandraConnectionStub::executeSelect(
    const char* /*queryString*/,
    nx::utils::MoveOnlyFunc<void(CassError, nx::cassandra::QueryResult result)> completionHandler)
{
    m_aioBinder.post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            completionHandler(
                CassError::CASS_ERROR_LIB_NOT_IMPLEMENTED,
                nx::cassandra::QueryResult());
        });
}

void CassandraConnectionStub::executeUpdate(
    nx::cassandra::Query /*query*/,
    nx::utils::MoveOnlyFunc<void(CassError)> completionHandler)
{
    m_aioBinder.post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            completionHandler(CassError::CASS_ERROR_LIB_NOT_IMPLEMENTED);
        });
}

void CassandraConnectionStub::executeUpdate(
    const char* /*queryString*/,
    nx::utils::MoveOnlyFunc<void(CassError)> completionHandler)
{
    m_aioBinder.post(
        [this, completionHandler = std::move(completionHandler)]()
        {
            completionHandler(
                m_isConnected
                    ? CassError::CASS_OK
                    : CassError::CASS_ERROR_LIB_NOT_IMPLEMENTED);
        });
}

void CassandraConnectionStub::wait()
{
    m_aioBinder.pleaseStopSync();
}

void CassandraConnectionStub::setDbHostAvailable(bool val)
{
    m_isDbHostAvailable = val;
}

void CassandraConnectionStub::setInitializationDoneEventQueue(
    nx::utils::SyncQueue<bool /*initialization result*/>* queue)
{
    m_initializationDoneEventQueue = queue;
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
