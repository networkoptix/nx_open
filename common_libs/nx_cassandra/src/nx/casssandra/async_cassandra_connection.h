#pragma once

#include <string>

#include <cassandra.h>

#include <boost/optional/optional.hpp>
#include <nx/utils/basic_factory.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace nx {
namespace cassandra {

struct InetAddr
{
    std::string addrString;
    InetAddr(const std::string& addrString): addrString(addrString) {}
    InetAddr() = default;
};

struct Uuid
{
    std::string uuidString;
    Uuid(const std::string& uuidString) : uuidString(uuidString) {}
    Uuid() = default;
};

class NX_CASSANDRA_API Query
{
public:
    Query(CassFuture* future);
    Query(CassStatement* statement);
    Query() = default;
    ~Query();

    Query(const Query&) = delete;
    Query& operator=(const Query&) = delete;

    Query(Query&&);
    Query& operator=(Query&&);


    bool bind(const std::string& key, const std::string& value);
    bool bind(const std::string& key, const char* value);
    bool bind(const std::string& key, bool value);
    bool bind(const std::string& key, double value);
    bool bind(const std::string& key, float value);
    bool bind(const std::string& key, int32_t value);
    bool bind(const std::string& key, int64_t value);
    bool bind(const std::string& key, const InetAddr& value);
    bool bind(const std::string& key, const Uuid& value);
    bool bindNull(const std::string& key);

private:
    const CassPrepared* m_prepared = nullptr;
    CassStatement* m_statement = nullptr;

    friend class AsyncConnection;
};

class NX_CASSANDRA_API QueryResult
{
public:
    QueryResult(CassFuture* future);
    QueryResult() = default;
    ~QueryResult();

    QueryResult(const QueryResult&) = delete;
    QueryResult& operator=(const QueryResult&) = delete;

    QueryResult(QueryResult&&);
    QueryResult& operator=(QueryResult&&);

    bool next();

    /**
     * These functions return true if query itself is successful. If function returned true, but
     * 'value' is equal to boost::none, then corresponding DB value is equal to NULL.
     */
    bool get(const std::string& key, boost::optional<std::string>* value) const;
    bool get(const std::string& key, boost::optional<bool>* value) const;
    bool get(const std::string& key, boost::optional<double>* value) const;
    bool get(const std::string& key, boost::optional<float>* value) const;
    bool get(const std::string& key, boost::optional<int32_t>* value) const;
    bool get(const std::string& key, boost::optional<int64_t>* value) const;
    bool get(const std::string& key, boost::optional<InetAddr>* value) const;
    bool get(const std::string& key, boost::optional<Uuid>* value) const;

    bool get(int index, boost::optional<std::string>* value) const;
    bool get(int index, boost::optional<bool>* value) const;
    bool get(int index, boost::optional<double>* value) const;
    bool get(int index, boost::optional<float>* value) const;
    bool get(int index, boost::optional<int32_t>* value) const;
    bool get(int index, boost::optional<int64_t>* value) const;
    bool get(int index, boost::optional<InetAddr>* value) const;
    bool get(int index, boost::optional<Uuid>* value) const;

private:
    const CassResult* m_result = nullptr;
    CassIterator* m_iterator = nullptr;

#define NX_GET_CASS_VALUE(getIndexFunc, cassValue, userValue) \
    auto getCassValueResult = getCassValueFromIterator(getIndexFunc, &cassValue, userValue); \
    if (cassValue == nullptr || *userValue == boost::none) \
        return getCassValueResult;


    template<typename GetIndexFunc, typename T>
    bool getCassValueFromIterator(
        GetIndexFunc getIndexFunc,
        const CassValue** cassValue,
        boost::optional<T>* userValue) const
    {
        *cassValue = nullptr;
        if (!m_iterator)
            return false;

        const CassRow* row = cass_iterator_get_row(m_iterator);
        if (!row)
            return false;

        *cassValue = getIndexFunc(row);
        if (cass_value_is_null(*cassValue))
        {
            *userValue = boost::none;
            return true;
        }

        *userValue = T();
        return true;
    }

    template<typename GetCassValueFunc>
    bool getStringImpl(GetCassValueFunc getCassValueFunc, boost::optional<std::string>* value) const
    {
        const CassValue* cassValue;
        NX_GET_CASS_VALUE(getCassValueFunc, cassValue, value);

        const char* stringVal;
        size_t stringValSize;
        auto result = cass_value_get_string(cassValue, &stringVal, &stringValSize);

        if (result == CASS_OK)
        {
            (*value)->resize(stringValSize);
            memcpy((void*)((*value)->data()), stringVal, stringValSize);
        }

        return result == CASS_OK;
    }

    template<typename GetCassValueFunc>
    bool getInetImpl(GetCassValueFunc getCassValueFunc, boost::optional<InetAddr>* value) const
    {
        const CassValue* cassValue;
        NX_GET_CASS_VALUE(getCassValueFunc, cassValue, value);

        CassInet val;
        auto result = cass_value_get_inet(cassValue, &val);

        if (result == CASS_OK)
        {
            (*value)->addrString.resize(CASS_INET_STRING_LENGTH);
            cass_inet_string(val, (char*)((*value)->addrString.data()));
        }

        return result == CASS_OK;
    }

    template<typename GetCassValueFunc>
    bool getUuidImpl(GetCassValueFunc getCassValueFunc, boost::optional<Uuid>* value) const
    {
        const CassValue* cassValue;
        NX_GET_CASS_VALUE(getCassValueFunc, cassValue, value);

        CassUuid val;
        auto result = cass_value_get_uuid(cassValue, &val);

        if (result == CASS_OK)
        {
            (*value)->uuidString.resize(CASS_UUID_STRING_LENGTH);
            cass_uuid_string(val, (char*)((*value)->uuidString.data()));
        }

        return result == CASS_OK;
    }

#undef NX_GET_CASS_VALUE
};

//-------------------------------------------------------------------------------------------------

class NX_CASSANDRA_API AbstractAsyncConnection
{
public:
    virtual ~AbstractAsyncConnection() = default;

    virtual void init(nx::utils::MoveOnlyFunc<void(CassError)> completionHandler) = 0;

    virtual void prepareQuery(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError, Query query)> completionHandler) = 0;

    virtual void executeSelect(
        Query query,
        nx::utils::MoveOnlyFunc<void(CassError, QueryResult result)> completionHandler) = 0;

    virtual void executeSelect(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError, QueryResult result)> completionHandler) = 0;

    virtual void executeUpdate(
        Query query,
        nx::utils::MoveOnlyFunc<void(CassError)> completionHandler) = 0;

    virtual void executeUpdate(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError)> completionHandler) = 0;

    virtual void wait() = 0;

    /**
     * Following methods are implemented on top of pure virtual ones.
     */

    cf::future<CassError> init();
    cf::future<std::pair<CassError, Query>> prepareQuery(const char* queryString);

    cf::future<std::pair<CassError, QueryResult>> executeSelect(Query query);
    cf::future<std::pair<CassError, QueryResult>> executeSelect(const char* queryString);

    cf::future<CassError> executeUpdate(Query query);
    cf::future<CassError> executeUpdate(const char* queryString);
};

class NX_CASSANDRA_API AsyncConnection:
    public AbstractAsyncConnection
{
public:
    AsyncConnection(const std::string& host);
    ~AsyncConnection();

    AsyncConnection(const AsyncConnection&) = delete;
    AsyncConnection& operator=(const AsyncConnection&) = delete;

    AsyncConnection(AsyncConnection&&) = default;
    AsyncConnection& operator=(AsyncConnection&&) = default;

    using AbstractAsyncConnection::init;
    using AbstractAsyncConnection::prepareQuery;
    using AbstractAsyncConnection::executeSelect;
    using AbstractAsyncConnection::executeUpdate;

    virtual void init(nx::utils::MoveOnlyFunc<void(CassError)> completionHandler);

    virtual void prepareQuery(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError, Query query)> completionHandler) override;

    virtual void executeSelect(
        Query query,
        nx::utils::MoveOnlyFunc<void(CassError, QueryResult result)> completionHandler) override;

    virtual void executeSelect(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError, QueryResult result)> completionHandler) override;

    virtual void executeUpdate(
        Query query,
        nx::utils::MoveOnlyFunc<void(CassError)> completionHandler) override;

    virtual void executeUpdate(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError)> completionHandler) override;

    virtual void wait() override;

private:
    CassCluster* m_cluster = nullptr;
    CassSession* m_session = nullptr;
    QnMutex m_mutex;
    QnWaitCondition m_cond;
    int m_pendingCount = 0;

    void incPending();
    void decPending();

    static void onDone(CassFuture* future, void* data);
    static void onPrepare(CassFuture* future, void* data);
    static void onSelect(CassFuture* future, void* data);
    static void onUpdate(CassFuture* future, void* data);
};

//-------------------------------------------------------------------------------------------------

using AsyncConnectionFactoryFunction =
    std::unique_ptr<AbstractAsyncConnection>(const std::string& dbHostName);

class NX_CASSANDRA_API AsyncConnectionFactory:
    public nx::utils::BasicFactory<AsyncConnectionFactoryFunction>
{
    using base_type = nx::utils::BasicFactory<AsyncConnectionFactoryFunction>;

public:
    AsyncConnectionFactory();

    static AsyncConnectionFactory& instance();

private:
    std::unique_ptr<AbstractAsyncConnection> defaultFactoryFunction(const std::string& dbHostName);
};

} // namespace cassandra
} // namespace nx
