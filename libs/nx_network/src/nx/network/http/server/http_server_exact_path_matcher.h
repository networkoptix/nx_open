// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <optional>
#include <map>
#include <vector>

#include "../http_types.h"

namespace nx {
namespace network {
namespace http {

template<typename Mapped>
class ExactPathMatcher
{
public:
    bool add(const std::string_view& path, Mapped mapped)
    {
        return m_pathToMapped.emplace(std::string(path), std::move(mapped)).second;
    }

    std::optional<std::reference_wrapper<const Mapped>> match(
        const std::string_view& path,
        RequestPathParams* /*pathParams*/,
        std::string* /*pathTemplate*/) const
    {
        auto it = m_pathToMapped.find(path);
        if (it == m_pathToMapped.end())
            return std::nullopt;

        return std::cref(it->second);
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

} // namespace nx
} // namespace network
} // namespace http
