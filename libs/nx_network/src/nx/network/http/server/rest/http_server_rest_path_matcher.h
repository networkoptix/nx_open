// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <optional>
#include <regex>
#include <string>

#include "../../http_types.h"

namespace nx::network::http::server::rest {

/**
 * Usage example:
 * - Register path "/account/{accountId}/systems".
 * - PathMatcher::match("/account/vpupkin/systems") matches to the registered path
 *   and adds "vpupkin" to pathParams.
 *
 * NOTE: Empty parameter is not matched.
 * E.g., PathMatcher::match("/account//systems") returns std::nullopt.
 */
template<typename Mapped>
class PathMatcher
{
public:
    /**
     * Registers path that may contain REST parameters. Each parameter is a string token
     * enclosed into {}. E.g., {name}. A path template may contain zero or more REST parameters.
     * @return true if path registered. false if a duplicate or an invalid path template.
     */
    bool add(const std::string_view& pathTemplate, Mapped mapped)
    {
        MatchContext matchContext;
        matchContext.regex = convertToRegex(pathTemplate);
        if (!fetchParamNames(pathTemplate, &matchContext.paramNames))
            return false;

        matchContext.mapped = std::move(mapped);

        if (std::any_of(
            m_restPathToMatchContext.begin(), m_restPathToMatchContext.end(),
            [&pathTemplate](const auto& elem) { return elem.first == pathTemplate; }))
        {
            // Duplicate entry.
            return false;
        }

        m_restPathToMatchContext.emplace_back(
            std::string(pathTemplate),
            std::move(matchContext));
        return true;
    }

    /**
     * Matches registered paths. Path templates are tested in the order
     * they were registered using PathMatcher::add.
     */
    std::optional<std::reference_wrapper<const Mapped>> match(
        const std::string_view& path,
        RequestPathParams* pathParams,
        std::string* pathTemplate) const
    {
        for (const auto& [pathTemplateStr, matchContext] : m_restPathToMatchContext)
        {
            std::match_results<std::string_view::const_iterator> matchResult;
            if (std::regex_search(path.begin(), path.end(), matchResult, matchContext.regex))
            {
                RequestPathParams params;
                bool matched = true;
                for (size_t i = 1; i < matchResult.size(); ++i)
                {
                    if (matchResult[i].length() == 0)
                    {
                        matched = false;
                        break;
                    }
                    params.nameToValue.emplace(
                        matchContext.paramNames[i - 1], matchResult[i]);
                }
                if (!matched)
                    continue;
                *pathParams = std::move(params);
                *pathTemplate = pathTemplateStr;
                return std::cref(matchContext.mapped);
            }
        }

        return std::nullopt;
    }

private:
    struct MatchContext
    {
        std::regex regex;
        /**
         * NOTE: Order preserves the order in the request.
         */
        std::vector<std::string> paramNames;
        Mapped mapped;
    };

    /** REST path template, context */
    std::vector<std::pair<std::string, MatchContext>> m_restPathToMatchContext;

    std::regex convertToRegex(const std::string_view& pathTemplate)
    {
        const std::regex replaceRestParams(
            "{[0-9a-zA-Z_-]*}", std::regex_constants::basic);
        const std::string replacement("\\([^/]*\\)");

        std::string restPathMatchRegex;
        restPathMatchRegex += "^";
        std::regex_replace(
            std::back_inserter(restPathMatchRegex),
            pathTemplate.begin(), pathTemplate.end(),
            replaceRestParams,
            replacement);
        restPathMatchRegex += "$";

        return std::regex(
            std::move(restPathMatchRegex),
            std::regex::icase | std::regex_constants::basic);
    }

    bool fetchParamNames(const std::string_view& pathTemplateView, std::vector<std::string>* names)
    {
        std::string pathTemplate(pathTemplateView);
        std::set<std::string> uniqueNames;

        const std::regex findRestParamsRegex(
            "{\\([0-9a-zA-Z_-]*\\)}", std::regex_constants::basic);

        std::smatch matchResult;
        while (std::regex_search(pathTemplate, matchResult, findRestParamsRegex))
        {
            for (size_t i = 1; i < matchResult.size(); ++i)
            {
                names->push_back(matchResult[i].str());
                if (!uniqueNames.emplace(matchResult[i].str()).second)
                    return false;
            }
            pathTemplate = matchResult.suffix();
        }

        return true;
    }
};

} // namespace nx::network::http::server::rest
