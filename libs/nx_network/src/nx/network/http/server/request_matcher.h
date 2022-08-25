// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>

#include "../http_types.h"

namespace nx::network::http {

template<typename Value>
struct RequestMatchResult
{
    const Value& value;
    std::string_view pathTemplate;
    RequestPathParams pathParams;
};

/**
 * Associative container that associates (HTTP method; request path) to some value.
 * Path is matched by a separate type which can provide support for REST request paths.
 */
template<typename Mapped, template<typename> class PathMatcher>
class RequestMatcher
{
public:
    using MatchResult = RequestMatchResult<Mapped>;

    /**
     * Register new request.
     * @param method Empty method adds default value which is used when no specific method matched.
     * @param path If empty, then adds default value for the given method. This default value
     * is matched when value with non-empty path was not found.
     * @return true if registered, false if such (method; path) is already registered.
     */
    template<typename MappedRef>
    bool add(
        const Method& method,
        const std::string_view& path,
        MappedRef&& mapped)
    {
        PathMatchContext& ctx = m_methodToPath[method];
        if (path.empty())
        {
            if (ctx.defaultItem)
                return false;
            ctx.defaultItem = std::forward<MappedRef>(mapped);
            return true;
        }

        return ctx.pathToItem.add(path, std::forward<MappedRef>(mapped));
    }

    /**
     * @return Value that corresponds to the given (method; path). std::nullopt if nothing was matched.
     */
    std::optional<MatchResult> match(
        const Method& method,
        const std::string_view& path) const
    {
        if (auto res = matchInternal(method, path); res)
            return res;

        if (method != Method(""))
        {
            // Not matched, trying to match default value.
            if (auto res = matchInternal(Method(""), path); res)
                return res;
        }

        return std::nullopt;
    }

    void clear()
    {
        m_methodToPath.clear();
    }

private:
    struct PathMatchContext
    {
        PathMatcher<Mapped> pathToItem;
        std::optional<Mapped> defaultItem;
    };

    std::optional<MatchResult> matchInternal(
        const Method& method,
        const std::string_view& path) const
    {
        if (auto ctxIter = m_methodToPath.find(method); ctxIter != m_methodToPath.end())
            return matchPath(ctxIter->second, path);

        return std::nullopt;
    }

    std::optional<MatchResult> matchPath(
        const PathMatchContext& pathMatchContext,
        const std::string_view& path) const
    {
        if (auto result = pathMatchContext.pathToItem.match(path); result)
            return result;

        if (pathMatchContext.defaultItem)
        {
            return MatchResult{
                .value = *pathMatchContext.defaultItem,
                .pathTemplate = {},
                .pathParams = {}};
        }

        return std::nullopt;
    }

private:
    std::unordered_map<Method, PathMatchContext> m_methodToPath;
};

} // namespace nx::network::http
