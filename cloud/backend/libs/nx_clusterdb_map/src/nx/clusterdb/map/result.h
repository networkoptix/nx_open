#pragma once

#include <string>

namespace nx::clusterdb::map {

enum class ResultCode
{
    ok = 0,
    notFound,
    logicError,
    dbError,
    unknownError,
};

struct NX_KEY_VALUE_DB_API Result
{
    ResultCode code;
    std::string text;

    Result(ResultCode code = ResultCode::unknownError, const std::string& text = std::string());

    bool ok() const;
    std::string toString() const;
};

NX_KEY_VALUE_DB_API const char* toString(ResultCode result);

} // namespace nx::clusterdb::map