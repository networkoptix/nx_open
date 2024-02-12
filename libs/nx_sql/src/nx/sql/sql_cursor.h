// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>

#include <nx/utils/std/optional.h>
#include <nx/utils/uuid.h>

#include "async_sql_query_executor.h"

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
    Cursor(AbstractAsyncSqlQueryExecutor* asyncSqlQueryExecutor, nx::Uuid id):
        m_asyncSqlQueryExecutor(asyncSqlQueryExecutor),
        m_id(id)
    {
    }

    virtual ~Cursor()
    {
        m_asyncSqlQueryExecutor->removeCursor(m_id);
    }

    std::optional<Record> next()
    {
        std::promise<std::tuple<DBResult, Record>> recordFetched;
        m_asyncSqlQueryExecutor->fetchNextRecordFromCursor<Record>(
            m_id,
            [&recordFetched](DBResult resultCode, Record record)
            {
                recordFetched.set_value(std::make_tuple(resultCode, std::move(record)));
            });
        auto result = recordFetched.get_future().get();

        if (std::get<0>(result) == DBResultCode::ok)
            return std::get<1>(result);

        return std::nullopt;
    }

private:
    AbstractAsyncSqlQueryExecutor* m_asyncSqlQueryExecutor;
    const nx::Uuid m_id;
};

} // namespace nx::sql
