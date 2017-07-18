#pragma once

#include <string>
#include <cassandra.h>
#include <boost/optional/optional.hpp>
#include <nx/utils/move_only_func.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/thread/mutex.h>

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

    CassRow* nextRow() const;

    template<typename GetIndexFunc>
    bool getStringImpl(GetIndexFunc getIndexFunc, boost::optional<std::string>* value) const
    {
        if (!m_iterator)
            return false;

        const CassRow* row = cass_iterator_get_row(m_iterator);
        if (!row)
            return false;

        const CassValue* cassValue = getIndexFunc(row);
        if (cass_value_is_null(cassValue))
        {
            *value = boost::none;
            return true;
        }

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

    template<typename GetIndexFunc>
    bool getInetImpl(GetIndexFunc getIndexFunc, boost::optional<InetAddr>* value) const
    {
        if (!m_iterator)
            return false;

        const CassRow* row = cass_iterator_get_row(m_iterator);
        if (!row)
            return false;

        const CassValue* cassValue = getIndexFunc(row);
        if (cass_value_is_null(cassValue))
        {
            *value = boost::none;
            return true;
        }

        CassInet val;
        auto result = cass_value_get_inet(cassValue, &val);

        if (result == CASS_OK)
        {
            (*value)->addrString.resize(CASS_INET_STRING_LENGTH);
            cass_inet_string(val, (char*)((*value)->addrString.data()));
        }

        return result == CASS_OK;
    }

    template<typename GetIndexFunc>
    bool getUuidImpl(GetIndexFunc getIndexFunc, boost::optional<Uuid>* value) const
    {
        if (!m_iterator)
            return false;

        const CassRow* row = cass_iterator_get_row(m_iterator);
        if (!row)
            return false;

        const CassValue* cassValue = getIndexFunc(row);
        if (cass_value_is_null(cassValue))
        {
            *value = boost::none;
            return true;
        }

        CassUuid val;
        auto result = cass_value_get_uuid(cassValue, &val);

        if (result == CASS_OK)
        {
            (*value)->uuidString.resize(CASS_UUID_STRING_LENGTH);
            cass_uuid_string(val, (char*)value->uuidString.data());
        }

        return result == CASS_OK;
    }
};

class NX_CASSANDRA_API AsyncConnection
{
public:
    AsyncConnection(const char* host);
    ~AsyncConnection();

    AsyncConnection(const AsyncConnection&) = delete;
    AsyncConnection& operator=(const AsyncConnection&) = delete;

    AsyncConnection(AsyncConnection&&) = default;
    AsyncConnection& operator=(AsyncConnection&&) = default;

    void init(nx::utils::MoveOnlyFunc<void(CassError)> initCb);

    void prepareQuery(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError, Query query)> prepareCb);

    void executeSelect(
        Query query,
        nx::utils::MoveOnlyFunc<void(CassError, QueryResult result)> selectCb);

    void executeSelect(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError, QueryResult result)> selectCb);

    void executeUpdate(
        Query query,
        nx::utils::MoveOnlyFunc<void(CassError)> updateCb);

    void executeUpdate(
        const char* queryString,
        nx::utils::MoveOnlyFunc<void(CassError)> updateCb);

    cf::future<CassError> init();
    cf::future<std::pair<CassError, Query>> prepareQuery(const char* queryString);

    cf::future<std::pair<CassError, QueryResult>> executeSelect(Query query);
    cf::future<std::pair<CassError, QueryResult>> executeSelect(const char* queryString);

    cf::future<CassError> executeUpdate(Query query);
    cf::future<CassError> executeUpdate(const char* queryString);

private:
    CassCluster* m_cluster = nullptr;
    CassSession* m_session = nullptr;
    QnMutex m_mutex;
};

} // namespace cassandra
} // namespace nx
