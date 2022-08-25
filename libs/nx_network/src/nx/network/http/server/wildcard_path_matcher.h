// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <vector>
#include <utility>

#include <nx/utils/match/wildcard.h>

#include "request_matcher.h"

namespace nx::network::http {

/**
 * Matches path against registered wildcard masks. Masks are tested in decreasing mask length order.
 * It is considered that matching a mask with bigger length is more accurate match.
 * Masks of the same length are tested in the order they were added.
 */
template<typename Mapped>
class WildcardPathMatcher
{
public:
    using MatchResult = RequestMatchResult<Mapped>;

    /**
     * Note: call to this method invalidates all previously returned MatchResult.
     * Complexity: linear.
     */
    template<typename MappedRef>
    bool add(const std::string_view& pathMask, MappedRef&& mapped)
    {
        auto existingMaskIter = std::find_if(m_masks.begin(), m_masks.end(),
            [&pathMask](const Context& ctx) { return ctx.mask == pathMask; });
        if (existingMaskIter != m_masks.end())
            return false;

        auto comp = [](const Context& one, const Context& two) {
            return one.mask.size() > two.mask.size();
        };

        Context newValue{std::string(pathMask), std::forward<MappedRef>(mapped)};
        auto it = std::upper_bound(m_masks.begin(), m_masks.end(), newValue, comp);
        m_masks.insert(it, std::move(newValue));
        return true;
    }

    /**
     * Wildcard masks are matched in decreasing mask length order.
     * Complexity: linear.
     * Note: Aho-Corasick algorithm can be used to speed it up if performance issues will arise
     * (which is unlikely since number of path masks is usually low).
     */
    std::optional<MatchResult> match(const std::string_view& path) const
    {
        for (const auto& val: m_masks)
        {
            if (wildcardMatch(val.mask, path))
            {
                return MatchResult{
                    .value = val.mapped,
                    .pathTemplate = (std::string_view) val.mask,
                    .pathParams = {}};
            }
        }

        return std::nullopt;
    }

private:
    struct Comparator
    {
        bool operator()(const std::string& left, const std::string& right) const
        {
            return left.size() != right.size()
                ? left.size() > right.size()
                : left < right;
        }
    };

    struct Context
    {
        std::string mask;
        Mapped mapped;
    };

    std::vector<Context> m_masks;
};

} // namespace nx::network::http
