// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <nx/utils/string.h>

#include "query_context.h"

namespace nx::sql {

static std::optional<nx::utils::SoftwareVersion> fetchVersion(
    QueryContext* qctx,
    const std::string& sqlText)
{
    auto query = qctx->connection()->createQuery();
    query->prepare(sqlText);
    query->exec();

    if (!query->next())
        return std::nullopt;

    return detail::parseVersion(query->value(0).toString().toStdString());
}

static std::optional<Info> fetchSqliteInfo(QueryContext* qctx)
{
    return Info{
        // Example: "3.11.0".
        .version = fetchVersion(qctx, "SELECT sqlite_version()")};
}

static std::optional<Info> fetchMysqlInfo(QueryContext* qctx)
{
    return Info{
        // Example: "5.7.38-log".
        .version = fetchVersion(qctx, "SELECT version()")};
}

std::optional<Info> fetchRdbmsInfo(QueryContext* qctx)
{
    switch (qctx->connection()->driverType())
    {
        case RdbmsDriverType::sqlite:
            return fetchSqliteInfo(qctx);
        case RdbmsDriverType::mysql:
            return fetchMysqlInfo(qctx);
        default:
            return std::nullopt;
    }
}

//-------------------------------------------------------------------------------------------------

namespace detail {

std::optional<nx::utils::SoftwareVersion> parseVersion(
    std::string_view text)
{
    const auto [tokens, count] = nx::utils::split_n<3>(text, '.');

    if (count < 2)
        return std::nullopt; //< Major and minor are required.

    std::size_t ok = 0;

    const int major = nx::utils::stoi(tokens[0], &ok);
    if (!ok)
        return std::nullopt; // Could not read major.

    const int minor = nx::utils::stoi(tokens[1], &ok);
    if (!ok)
        return std::nullopt; // Could not read minor.

    int build = 0;
    if (count > 2)
        build = nx::utils::stoi(tokens[2], &ok);

    return nx::utils::SoftwareVersion(major, minor, build);
}

} // namespace detail

} // namespace nx::sql
