#pragma once

#include <future>

#include <boost/optional.hpp>

#include <nx/utils/uuid.h>

namespace nx::sql {

class AsyncSqlQueryExecutor;

class BaseCursor
{
public:
    virtual ~BaseCursor() = default;
};

template<typename Record>
class Cursor:
    public BaseCursor
{
public:
    Cursor(AsyncSqlQueryExecutor* asyncSqlQueryExecutor, QnUuid id):
        m_asyncSqlQueryExecutor(asyncSqlQueryExecutor),
        m_id(id)
    {
    }

    virtual ~Cursor()
    {
        m_asyncSqlQueryExecutor->removeCursor(m_id);
    }

    boost::optional<Record> next()
    {
        std::promise<std::tuple<DBResult, Record>> recordFetched;
        m_asyncSqlQueryExecutor->fetchNextRecordFromCursor<Record>(
            m_id,
            [this, &recordFetched](DBResult resultCode, Record record)
            {
                recordFetched.set_value(std::make_tuple(resultCode, std::move(record)));
            });
        auto result = recordFetched.get_future().get();

        if (std::get<0>(result) == DBResult::ok)
            return std::get<1>(result);

        return boost::none;
    }

private:
    AsyncSqlQueryExecutor* m_asyncSqlQueryExecutor;
    const QnUuid m_id;
};

} // namespace nx::sql
