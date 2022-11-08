// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>

#include <nx/utils/software_version.h>

namespace nx::sql {

class QueryContext;

struct Info
{
    std::optional<nx::utils::SoftwareVersion> version;
};

/**
 * Read DB info.
 * Note: implemented for some DB types only.
 * @return DB version as reported by the DB. std::nullopt in case if DB type is not supported.
 * Note: throws on error.
 */
NX_SQL_API std::optional<Info> fetchRdbmsInfo(QueryContext* qctx);

//-------------------------------------------------------------------------------------------------

namespace detail {

/**
 * Parses the following ABNF:
 * VERSION = MAJOR "." MINOR ["." BUILD ] [ COMMENT ]
 * MAJOR = 1*DIGIT
 * MINOR = 1*DIGIT
 * BUILD = 1*DIGIT
 * COMMENT = TEXT
 */
NX_SQL_API std::optional<nx::utils::SoftwareVersion> parseVersion(std::string_view text);

} // namespace detail

} // namespace nx::sql
