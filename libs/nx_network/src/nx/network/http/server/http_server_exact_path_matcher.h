// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <optional>
#include <map>
#include <vector>

#include "request_matcher.h"
#include "../http_types.h"

namespace nx {
namespace network {
namespace http {

template<typename Mapped>
class ExactPathMatcher
{
public:
    using MatchResult = RequestMatchResult<Mapped>;

    bool add(const std::string_view& path, Mapped mapped)
    {
        return m_pathToMapped.emplace(std::string(path), std::move(mapped)).second;
    }

    std::optional<MatchResult> match(const std::string_view& path) const
    {
        if (auto it = m_pathToMapped.find(path); it != m_pathToMapped.end())
        {
            return MatchResult{
                .value = it->second,
                .pathTemplate = (std::string_view) it->first,
                .pathParams = {}};
        }

        return std::nullopt;
    }

private:
    // NOTE: Introduced to enable lookup by std::string_view
    struct Comparator
    {
        using is_transparent = std::true_type;

        template<typename Left, typename Right>
        bool operator()(const Left& left, const Right& right) const
        {
            return left < right;
        }
    };

    std::map<std::string, Mapped, Comparator> m_pathToMapped;
};

} // namespace http
} // namespace network
} // namespace nx
