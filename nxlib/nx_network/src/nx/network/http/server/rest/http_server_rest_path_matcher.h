#pragma once

#include <map>
#include <regex>
#include <string>

#include <boost/optional.hpp>

#include "../../http_types.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace rest {

/**
 * Usage example:
 * - Register path "/account/{accountId}/systems".
 * - PathMatcher::match("/account/vpupkin/systems") matches to the registered path
 *   and adds "vpupkin" to pathParams.
 *
 * NOTE: Empty parameter is not matched.
 *   E.g., PathMatcher::match("/account//systems") returns boost::none.
 */
template<typename Mapped>
class PathMatcher
{
public:
    bool add(const std::string& pathTemplate, Mapped mapped)
    {
        MatchContext matchContext;
        matchContext.regex = convertToRegex(pathTemplate);
        if (!fetchParamNames(pathTemplate, &matchContext.paramNames))
            return false;
        matchContext.mapped = std::move(mapped);

        return m_restPathToMatchContext.emplace(
            pathTemplate,
            std::move(matchContext)).second;
    }

    boost::optional<const Mapped&> match(
        const std::string& path,
        RequestPathParams* pathParams) const
    {
        for (const auto& matchContext: m_restPathToMatchContext)
        {
            std::smatch matchResult;
            if (std::regex_search(path, matchResult, matchContext.second.regex))
            {
                RequestPathParams params;
                for (size_t i = 1; i < matchResult.size(); ++i)
                {
                    if (matchResult[i].length() == 0)
                        return boost::none;
                    params.nameToValue.emplace(
                        matchContext.second.paramNames[i-1], matchResult[i]);
                }
                *pathParams = std::move(params);
                return boost::optional<const Mapped&>(matchContext.second.mapped);
            }
        }

        return boost::none;
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
    std::map<std::string, MatchContext> m_restPathToMatchContext;

    std::regex convertToRegex(const std::string& pathTemplate)
    {
        const std::regex replaceRestParams(
            "{[0-9a-zA-Z_-]*}", std::regex_constants::basic);
        const std::string replacement("\\([^/]*\\)");

        std::string restPathMatchRegex;
        restPathMatchRegex += "^";
        restPathMatchRegex += std::regex_replace(
            pathTemplate,
            replaceRestParams,
            replacement);
        restPathMatchRegex += "$";

        return std::regex(
            std::move(restPathMatchRegex),
            std::regex::icase | std::regex_constants::basic);
    }

    bool fetchParamNames(std::string pathTemplate, std::vector<std::string>* names)
    {
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

} // namespace rest
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
